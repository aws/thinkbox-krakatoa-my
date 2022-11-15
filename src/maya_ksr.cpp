// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <boost/regex.hpp>

#include "krakatoasr_datatypes.hpp"
#include "maya_ksr.hpp"
#include <frantic/maya/maya_util.hpp>

#include <frantic/maya/particles/particles.hpp>

#include "KrakatoaRender.hpp"
#include "KrakatoaSettings.hpp"
#include "PRTFractal.hpp"
#include "PRTLoader.hpp"
#include "PRTSurface.hpp"
#include "PRTVolume.hpp"
#include "maya_progress_bar_interface.hpp"

#include <krakatoasr_renderer/params.hpp>
#include <krakatoasr_renderer/progress_logger.hpp>

#include <maya/M3dView.h>
#include <maya/MAnimControl.h>
#include <maya/MCommonRenderSettingsData.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MFnRenderLayer.h>
#include <maya/MFnTransform.h>
#include <maya/MPlugArray.h>
#include <maya/MRenderUtil.h>
#include <maya/MRenderView.h>
#include <maya/MSelectionList.h>

#include <frantic/maya/PRTMayaParticle.hpp>
#include <frantic/maya/attributes.hpp>
#include <frantic/maya/convert.hpp>
#include <frantic/maya/geometry/mesh.hpp>
#include <frantic/maya/graphics/maya_space.hpp>
#include <frantic/maya/particles/texture_evaluation_particle_istream.hpp>
#include <frantic/maya/util.hpp>

#include <frantic/channels/named_channel_data.hpp>
#include <frantic/geometry/trimesh3.hpp>
#include <frantic/graphics/color3f.hpp>
#include <frantic/graphics/transform4f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/logging/global_progress_logger.hpp>
#include <frantic/logging/logging_level.hpp>
#include <frantic/logging/progress_logger.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/rle_levelset_particle_istream.hpp>
#include <frantic/particles/streams/shared_particle_container_particle_istream.hpp>
#include <frantic/particles/streams/transformed_particle_istream.hpp>
#include <frantic/volumetrics/levelset/rle_level_set.hpp>

#include <krakatoa/particle_volume.hpp>

#include <boost/algorithm/string.hpp>
#include <boost/shared_ptr.hpp>

#include <cstdio>
#include <sstream>

using namespace frantic::graphics;
using namespace frantic::channels;
using namespace frantic::particles;
using namespace frantic::particles::streams;
using namespace frantic::geometry;
using namespace frantic::maya;

namespace {

inline float shutter_angle_iterator( float begin, float end, int current, int numSamples ) {
    if( numSamples < 2 )
        return ( end + begin ) / 2.0f;
    else
        return ( ( current * ( end - begin ) ) / ( numSamples - 1 ) ) + begin;
}

inline bool get_shutter_range( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                               float& shutterBegin, float& shutterEnd ) {
    bool enableMotionBlur =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "enableMotionBlur", currentContext );

    int layerMotionBlurOverride = maya_ksr::get_render_layer_setting(
        "motionBlur", currentContext ); // render layers have a way of overriding if motion blur is enabled.
    if( layerMotionBlurOverride != -1 ) {
        enableMotionBlur = layerMotionBlurOverride != 0;
    }

    if( enableMotionBlur ) {
        const double framesPerSecond = maya_util::get_fps();
        const float shutterAngle =
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "shutterAngle", currentContext );
        const float shutterValue = static_cast<float>( ( shutterAngle / 360.0f ) * ( 1.0f / framesPerSecond ) );
        const float motionBlurBias =
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "motionBlurBias", currentContext );
        shutterBegin = -( shutterValue ) / 2.0f;
        shutterEnd = shutterBegin + shutterValue;
        shutterBegin += shutterValue * 0.5f * motionBlurBias;
        shutterEnd += shutterValue * 0.5f * motionBlurBias;
    }

    return enableMotionBlur;
}

inline bool get_depth_of_field( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                float& sampleRate ) {
    const bool enableDepthOfField =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "enableDOF", currentContext );

    if( enableDepthOfField ) {
        sampleRate = frantic::maya::get_float_attribute( krakatoaSettingsNode, "sampleRateDOF", currentContext );
    }

    return enableDepthOfField;
}

frantic::tstring get_plug_keyed_name( const MPlug& plug ) {
    MFnDependencyNode owner( plug.node() );
    MFnAttribute attr( plug.attribute() );
    return frantic::maya::from_maya_t( owner.name() ) + _T( "." ) + frantic::maya::from_maya_t( attr.name() );
}

bool trace_input_mesh_plug( const MFnMesh& mesh, const frantic::tstring& inputMeshAttr, frantic::tstring& outMeshKey ) {
    MStatus status;
    MPlug meshPlug = mesh.findPlug( frantic::maya::to_maya_t( inputMeshAttr ), &status );

    if( !status )
        return false;

    // It seems that this is the only parameter that can change the display result of the input mesh, so we'll have to
    // avoid trying to instance smoothed meshes
    if( !frantic::maya::get_boolean_attribute( mesh, "displaySmoothMesh" ) ) {
        MPlugArray connections;
        bool hasConnections = meshPlug.connectedTo( connections, true, false, &status );
        if( status && hasConnections && connections.length() == 1 ) {
            outMeshKey = get_plug_keyed_name( connections[0] );
            return true;
        }
    }

    return false;
}

} // namespace

namespace maya_ksr {

mesh_context::mesh_context() {}

mesh_context::~mesh_context() {}

krakatoasr::triangle_mesh* mesh_context::get_shared_instance( const frantic::tstring& name ) {
    mesh_container::iterator it = m_meshes.find( name );

    if( it == m_meshes.end() ) {
        it = m_meshes.insert( std::make_pair( name, krakatoasr::triangle_mesh() ) ).first;
    }

    return &it->second;
}

void apply_scene_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                              krakatoasr::krakatoa_renderer& krakRenderer, mesh_context& sharedMeshContext ) {
    apply_global_resolution_node_to_renderer( currentContext, krakRenderer );
    apply_shader_to_renderer( krakatoaSettingsNode, currentContext, krakRenderer );
    apply_render_method_to_renderer( krakatoaSettingsNode, currentContext, krakRenderer );
    apply_general_settings_to_renderer( krakatoaSettingsNode, currentContext, krakRenderer );
    bool ignoreSceneLights =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "ignoreSceneLights", currentContext );

    if( !ignoreSceneLights ) {
        std::vector<MDagPath> lights;
        maya_util::find_nodes_with_type( MFn::kLight, lights );

        for( size_t i = 0; i < lights.size(); ++i ) {
            MFnLight mayaLight( lights[i] );
            apply_light_to_renderer( mayaLight, krakatoaSettingsNode, currentContext, krakRenderer );
        }
    }

    // PRT Objects
    {
        bool exportPRTLoaders =
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "exportPRTLoaders", currentContext );
        bool exportPRTFractals =
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "exportPRTFractals", currentContext );
        bool exportMayaParticles =
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "exportMayaParticles", currentContext );
        bool exportPRTVolumes =
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "exportPRTVolumes", currentContext );
        bool exportPRTSurfaces =
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "exportPRTSurfaces", currentContext );

        std::vector<MDagPath> paths;
        std::vector<MObject> nodes;

        maya_util::find_nodes_with_output_stream( paths, nodes );

        std::vector<MDagPath>::const_iterator pathIter;
        std::vector<MObject>::const_iterator nodeIter;
        for( pathIter = paths.begin(), nodeIter = nodes.begin(); pathIter != paths.end() && nodeIter != nodes.end();
             ++pathIter, ++nodeIter ) {
            MFnDependencyNode depNode( *nodeIter );
            if( ( exportPRTLoaders && depNode.typeId() == PRTLoader::typeId ) ||
                ( exportPRTFractals && depNode.typeId() == PRTFractal::typeId ) ||
                ( exportPRTVolumes && depNode.typeId() == PRTVolume::typeId ) ||
                ( exportPRTSurfaces && depNode.typeId() == PRTSurface::typeId ) ||
                ( exportMayaParticles && depNode.typeId() == PRTMayaParticle::typeId ) ) {
                MFnDagNode pathNode( *pathIter );
                apply_prt_object_to_renderer( pathNode, depNode, krakatoaSettingsNode, currentContext, krakRenderer );
            }
        }
    }

    bool exportMatteObjects =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "enableMatteObjects", currentContext );
    if( exportMatteObjects ) {
        std::vector<MDagPath> meshObjects;
        maya_util::find_nodes_with_type( MFn::kMesh, meshObjects );

        for( size_t i = 0; i < meshObjects.size(); ++i ) {
            apply_maya_mesh_to_renderer( meshObjects[i], krakatoaSettingsNode, currentContext, krakRenderer,
                                         sharedMeshContext );
        }
    }
}

void apply_global_resolution_node_to_renderer( const MDGContext& currentContext,
                                               krakatoasr::krakatoa_renderer& krakRenderer ) {
    MCommonRenderSettingsData renderSettings;
    MRenderUtil::getCommonRenderSettings( renderSettings );
    krakRenderer.set_render_resolution( renderSettings.width, renderSettings.height );
    krakRenderer.set_pixel_aspect_ratio( renderSettings.pixelAspectRatio );
}

void apply_shader_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                               krakatoasr::krakatoa_renderer& krakRenderer ) {
    MString shadingMode = frantic::maya::get_enum_attribute( krakatoaSettingsNode, "shadingMode", currentContext );

    // the renderer internally copies all data out of the shader, which is why we can get away with this
    if( shadingMode == "Isotropic" ) {
        krakatoasr::shader_isotropic isoShader;
        krakRenderer.set_shader( &isoShader );
    } else if( shadingMode == "Phong Surface" ) {
        krakatoasr::shader_phong phongShader;
        phongShader.set_specular_power(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "phongSpecularPower", currentContext ) );
        phongShader.set_specular_level(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "phongSpecularLevel", currentContext ) );
        phongShader.use_specular_power_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "usePhongSpecularPowerChannel", currentContext ) );
        phongShader.use_specular_level_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "usePhongSpecularLevelChannel", currentContext ) );
        krakRenderer.set_shader( &phongShader );
    } else if( shadingMode == "Henyey-Greenstein" ) {
        krakatoasr::shader_henyey_greenstein hgShader;
        hgShader.set_phase_eccentricity(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "hgEccentricity", currentContext ) );
        hgShader.use_phase_eccentricity_channel(
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "useHgEccentricityChannel", currentContext ) );
        krakRenderer.set_shader( &hgShader );
    } else if( shadingMode == "Schlick" ) {
        krakatoasr::shader_schlick schlickShader;
        schlickShader.set_phase_eccentricity(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "schlickEccentricity", currentContext ) );
        schlickShader.use_phase_eccentricity_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useSchlickEccentricityChannel", currentContext ) );
        krakRenderer.set_shader( &schlickShader );
    } else if( shadingMode == "Kajiya-Kay" ) {
        krakatoasr::shader_kajiya_kay kkShader;
        kkShader.set_specular_power(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "kkSpecularPower", currentContext ) );
        kkShader.set_specular_level(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "kkSpecularLevel", currentContext ) );
        kkShader.use_specular_power_channel(
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "useKkSpecularPowerChannel", currentContext ) );
        kkShader.use_specular_level_channel(
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "useKkSpecularLevelChannel", currentContext ) );
        krakRenderer.set_shader( &kkShader );
    } else if( shadingMode == "Marschner" ) {
        krakatoasr::shader_marschner marschnerShader;
        marschnerShader.set_specular_level(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "marschnerSpecularLevel", currentContext ) );
        marschnerShader.set_secondary_specular_level(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "marschnerSpecular2Level", currentContext ) );
        marschnerShader.set_specular_glossiness(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "marschnerSpecularGlossiness" ) );
        marschnerShader.set_secondary_specular_glossiness( frantic::maya::get_float_attribute(
            krakatoaSettingsNode, "marschnerSpecular2Glossiness", currentContext ) );
        marschnerShader.set_specular_shift(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "marschnerSpecularShift", currentContext ) );
        marschnerShader.set_secondary_specular_shift(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "marschnerSpecular2Shift", currentContext ) );
        marschnerShader.set_glint_level(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "marschnerGlintLevel", currentContext ) );
        marschnerShader.set_glint_size(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "marschnerGlintSize", currentContext ) );
        marschnerShader.set_glint_glossiness(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "marschnerGlintGlossiness", currentContext ) );
        marschnerShader.set_diffuse_level(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "marschnerDiffuseLevel", currentContext ) );
        marschnerShader.use_specular_level_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useMarschnerSpecularLevelChannel", currentContext ) );
        marschnerShader.use_secondary_specular_level_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useMarschnerSpecular2LevelChannel", currentContext ) );
        marschnerShader.use_specular_glossiness_channel(
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "useMarschnerSpecularGlossinessChannel" ) );
        marschnerShader.use_secondary_specular_glossiness_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useMarschnerSpecular2GlossinessChannel", currentContext ) );
        marschnerShader.use_specular_shift_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useMarschnerSpecularShiftChannel", currentContext ) );
        marschnerShader.use_secondary_specular_shift_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useMarschnerSpecular2ShiftChannel", currentContext ) );
        marschnerShader.use_glint_level_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useMarschnerGlintLevelChannel", currentContext ) );
        marschnerShader.use_glint_size_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useMarschnerGlintSizeChannel", currentContext ) );
        marschnerShader.use_glint_glossiness_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useMarschnerGlintGlossinessChannel", currentContext ) );
        marschnerShader.use_diffuse_level_channel( frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "useMarschnerDiffuseLevelChannel", currentContext ) );
        krakRenderer.set_shader( &marschnerShader );
    } else {
        krakatoasr::shader_isotropic isoShader;
        krakRenderer.set_shader( &isoShader );
        FF_LOG( stats ) << "Krakatoa MY: Unknown shader type \"" << shadingMode.asChar() << "\", using default."
                        << std::endl;
    }
}

void apply_render_method_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                      krakatoasr::krakatoa_renderer& krakRenderer ) {
    MString renderingMethod =
        frantic::maya::get_enum_attribute( krakatoaSettingsNode, "renderingMethod", currentContext );

    if( renderingMethod == "Particles" ) {
        krakRenderer.set_rendering_method( krakatoasr::METHOD_PARTICLE );
        MString drawPointFilter =
            frantic::maya::get_enum_attribute( krakatoaSettingsNode, "drawPointFilter", currentContext );

        if( drawPointFilter == "Nearest" ) {
            krakRenderer.set_draw_point_filter( krakatoasr::FILTER_NEAREST_NEIGHBOR );
        } else if( drawPointFilter == "Bilinear" ) {
            krakRenderer.set_draw_point_filter(
                krakatoasr::FILTER_BILINEAR,
                frantic::maya::get_int_attribute( krakatoaSettingsNode, "finalPassFilterSize", currentContext ) );
        } else if( drawPointFilter == "Bicubic" ) {
            krakRenderer.set_draw_point_filter( krakatoasr::FILTER_BICUBIC );
        } else {
            FF_LOG( stats ) << "Krakatoa MY: Unknown filter type \""
                            << frantic::strings::to_tstring( drawPointFilter.asChar() ) << "\", using default."
                            << std::endl;
        }

        MString selfShadowFilter =
            frantic::maya::get_enum_attribute( krakatoaSettingsNode, "selfShadowFilter", currentContext );

        if( selfShadowFilter == "Nearest" ) {
            krakRenderer.set_attenuation_lookup_filter( krakatoasr::FILTER_NEAREST_NEIGHBOR );
        } else if( selfShadowFilter == "Bilinear" ) {
            krakRenderer.set_attenuation_lookup_filter(
                krakatoasr::FILTER_BILINEAR,
                frantic::maya::get_int_attribute( krakatoaSettingsNode, "lightingPassFilterSize", currentContext ) );
        } else if( selfShadowFilter == "Bicubic" ) {
            krakRenderer.set_attenuation_lookup_filter( krakatoasr::FILTER_BICUBIC );
        } else {
            FF_LOG( stats ) << "Krakatoa MY: Unknown filter type \""
                            << frantic::strings::to_tstring( drawPointFilter.asChar() ) << "\", using default."
                            << std::endl;
        }

        const int numThreads = frantic::maya::get_int_attribute( krakatoaSettingsNode, "threadCount", currentContext );
        const float memFraction = frantic::maya::get_float_attribute(
            krakatoaSettingsNode, "frameBufferAvailableMemoryFraction", currentContext );

        krakRenderer.set_number_of_threads( numThreads );
        krakRenderer.set_frame_buffer_available_memory_fraction( memFraction );
    } else if( renderingMethod == "Voxels" ) {
        krakRenderer.set_rendering_method( krakatoasr::METHOD_VOXEL );
        krakRenderer.set_voxel_size(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "voxelSize", currentContext ) );
        krakRenderer.set_voxel_filter_radius(
            frantic::maya::get_int_attribute( krakatoaSettingsNode, "voxelFilterRadius", currentContext ) );
    } else {
        FF_LOG( stats ) << "Krakatoa MY: Unknown rendering method \""
                        << frantic::strings::to_tstring( renderingMethod.asChar() ) << "\", using default."
                        << std::endl;
    }
}

void apply_general_settings_to_renderer( const MFnDependencyNode& krakatoaSettingsNode,
                                         const MDGContext& currentContext,
                                         krakatoasr::krakatoa_renderer& krakRenderer ) {
    // Color override
    if( frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "overrideBG", currentContext ) ) {
        color3f bgColor = frantic::maya::get_color_attribute( krakatoaSettingsNode, "backgroundColor", currentContext );
        krakRenderer.set_background_color( bgColor.r, bgColor.g, bgColor.b );
    }

    // Absorbtion and Emission
    krakRenderer.use_absorption_color(
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "useAbsorption", currentContext ) );
    krakRenderer.use_emission(
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "useEmission", currentContext ) );
    krakRenderer.set_additive_mode(
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "forceAdditiveMode", currentContext ) );

    float finalPassDensity =
        frantic::maya::get_float_attribute( krakatoaSettingsNode, "finalPassDensity", currentContext );
    int finalPassDensityExponent =
        frantic::maya::get_int_attribute( krakatoaSettingsNode, "finalPassDensityExponent", currentContext );

    krakRenderer.set_density_per_particle( finalPassDensity );
    krakRenderer.set_density_exponent( finalPassDensityExponent );

    bool useEmissionStrength =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "useEmissionStrength", currentContext );
    if( useEmissionStrength ) {
        krakRenderer.set_emission_strength(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "emissionStrength", currentContext ) );
        krakRenderer.set_emission_strength_exponent(
            frantic::maya::get_int_attribute( krakatoaSettingsNode, "emissionStrengthExponent", currentContext ) );
    } else {
        krakRenderer.set_emission_strength( finalPassDensity );
        krakRenderer.set_emission_strength_exponent( finalPassDensityExponent );
    }

    // Density
    bool useLightingPassDensity =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "useLightingPassDensity", currentContext );
    if( useLightingPassDensity ) {
        krakRenderer.set_lighting_density_per_particle(
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "lightingPassDensity", currentContext ) );
        krakRenderer.set_lighting_density_exponent(
            frantic::maya::get_int_attribute( krakatoaSettingsNode, "lightingPassDensityExponent", currentContext ) );
    } else {
        krakRenderer.set_lighting_density_per_particle( finalPassDensity );
        krakRenderer.set_lighting_density_exponent( finalPassDensityExponent );
    }

    // DOF
    krakRenderer.enable_depth_of_field(
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "enableDOF", currentContext ) );

    // Matte
    krakRenderer.set_matte_renderer_supersampling(
        frantic::maya::get_int_attribute( krakatoaSettingsNode, "matteSuperSampling", currentContext ) );

    // Output Settings
    bool isSavedZdepthPass =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "saveZDepthPass", currentContext );
    bool isSavedNormalPass =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "saveNormalPass", currentContext );
    bool isSavedVelocityPass =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "saveVelocityPass", currentContext );
    bool isSavedOccludedPass =
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "saveOccludedPass", currentContext );

    krakRenderer.enable_z_depth_render( isSavedZdepthPass );
    krakRenderer.enable_normal_render( isSavedNormalPass );
    krakRenderer.enable_velocity_render( isSavedVelocityPass );

    if( krakRenderer.get_params()->renderingMethod == krakatoasr::METHOD_VOXEL && isSavedOccludedPass )
        MGlobal::displayWarning( "Occluded RGBA pass is not supported in \"voxel\" rendering mode.\n" );
    else
        krakRenderer.enable_occluded_rgba_render( isSavedOccludedPass );

    MString loggingLevelStr = frantic::maya::get_enum_attribute( krakatoaSettingsNode, "logLevel", currentContext );

    krakatoasr::logging_level_t loggingLevel;

    if( loggingLevelStr == MString( "none" ) )
        loggingLevel = krakatoasr::LOG_NONE;
    else if( loggingLevelStr == MString( "errors" ) )
        loggingLevel = krakatoasr::LOG_ERRORS;
    else if( loggingLevelStr == MString( "warnings" ) )
        loggingLevel = krakatoasr::LOG_WARNINGS;
    else if( loggingLevelStr == MString( "progress" ) )
        loggingLevel = krakatoasr::LOG_PROGRESS;
    else if( loggingLevelStr == MString( "stats" ) )
        loggingLevel = krakatoasr::LOG_STATS;
    else if( loggingLevelStr == MString( "debug" ) )
        loggingLevel = krakatoasr::LOG_DEBUG;
    else {
        FF_LOG( stats ) << "Unknown logging level \"" << frantic::strings::to_tstring( loggingLevelStr.asChar() )
                        << "\", using defaults." << std::endl;
        loggingLevel = krakatoasr::LOG_STATS;
    }

    krakatoasr::set_global_logging_level( loggingLevel );
}

void apply_current_camera_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                       krakatoasr::krakatoa_renderer& krakRenderer ) {
    MStatus status;
    M3dView currentView = M3dView::active3dView( &status );

    if( !status ) {
        FF_LOG( stats ) << "Krakatoa MY: Could not get an active 3d view!\n";
        return;
    }

    MDagPath camDagPath;
    currentView.getCamera( camDagPath );
    MFnCamera mayaCamera( camDagPath, &status );
    if( !status ) {
        FF_LOG( stats ) << "Krakatoa MY: Could not get the current render camera!\n";
        return;
    }

    apply_camera_to_renderer( mayaCamera, krakatoaSettingsNode, currentContext, krakRenderer );
}

void apply_chosen_camera_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                      krakatoasr::krakatoa_renderer& krakRenderer, MString camera ) {
    MStatus status;
    MSelectionList list;
    MGlobal::getSelectionListByName( camera, list );
    MDagPath camDagPath;
    list.getDagPath( 0, camDagPath );
    MFnCamera mayaCamera( camDagPath, &status );
    if( !status ) {
        FF_LOG( stats ) << "Krakatoa MY: Could not get the current render camera!\n";
        return;
    }

    apply_camera_to_renderer( mayaCamera, krakatoaSettingsNode, currentContext, krakRenderer );
}

/**
 * Sets all of the appropriate renderer parameters using the provided camera
 */
void apply_camera_to_renderer( const MFnCamera& mayaCamera, const MFnDependencyNode& krakatoaSettingsNode,
                               const MDGContext& currentContext, krakatoasr::krakatoa_renderer& krakRenderer ) {
    float inchToMM = 25.4f;
    float totalPerspectiveScale = inchToMM * 0.5f * (float)mayaCamera.cameraScale() / (float)mayaCamera.preScale() /
                                  (float)mayaCamera.postScale() / (float)mayaCamera.focalLength();

    float combinedScale = (float)mayaCamera.preScale() * (float)mayaCamera.postScale();

    krakRenderer.set_camera_clipping( (float)mayaCamera.nearClippingPlane(), (float)mayaCamera.farClippingPlane() );
    if( mayaCamera.isOrtho() ) {
        krakRenderer.set_camera_type( krakatoasr::CAMERA_ORTHOGRAPHIC );
        krakRenderer.set_camera_orthographic_width( mayaCamera.orthoWidth() );
    } else {
        krakRenderer.set_camera_type( krakatoasr::CAMERA_PERSPECTIVE );
    }

    // I'm hoping that this call will return the dag path that was used to create this function set
    MDagPath camDagPath;
    mayaCamera.getPath( camDagPath );
    MCommonRenderSettingsData renderSettings;
    MRenderUtil::getCommonRenderSettings( renderSettings );

    // frame of view angle - modified based off of Fit Resolution Gate values
    float perspective;

    // the total horizontal offset which is created through the Film Translate, Horizontal Film Offset and when the Fit
    // resolution gate setting is set to vertical the film Fit offset.  Measured in Pixels. All changes must be
    // multiplied by -1 to compensate for the way the code handles
    float horizontalOffset = 0;
    // the total horizontal offset which is created through the Film Translate, vertical Film Offset and when the Fit
    // resolution gate setting is set to horizontal the film Fit offset.  Measured in Pixels. All changes must be
    // multiplied by -1 to compensate for the way the code handles
    float verticalOffset = 0;

    // some commonly used values to reduce the size of the formulas
    float renderAspectRatio = (float)renderSettings.width / renderSettings.height;
    float aspectDifference = (float)mayaCamera.aspectRatio() * (float)mayaCamera.lensSqueezeRatio() - renderAspectRatio;

    // get total film offset from maya camera (regular offset plus "shake" offset)
    float mayaHorizFilmOffset = (float)mayaCamera.horizontalFilmOffset();
    float mayaVertFilmOffset = (float)mayaCamera.verticalFilmOffset();
    if( mayaCamera.shakeEnabled() ) {
        mayaHorizFilmOffset += (float)mayaCamera.horizontalShake();
        mayaVertFilmOffset += (float)mayaCamera.verticalShake();
    }

    if( mayaCamera.filmFit() == MFnCamera::kHorizontalFilmFit ) {
        // if the aspect ratios of the image and the film gate are different the image extends vertically beyond the
        // resolution gate and can be cropped calculating the field of view by ourselves since it is affected by the
        // lens squeeze ratio. 12.7 is conversion from inches to mm and divide by 2
        perspective = 2.0f * std::atan( totalPerspectiveScale * (float)mayaCamera.horizontalFilmAperture() *
                                        (float)mayaCamera.lensSqueezeRatio() );

        // Film Offset values - film offset is measured in Inches of film compared to the cameras aperture
        // eg. camera horizontal aperture of 1.5 film offset of 1 will move the everything 2/3 of a render width
        horizontalOffset =
            ( mayaHorizFilmOffset * renderSettings.width * combinedScale ) / (float)mayaCamera.horizontalFilmAperture();
        verticalOffset = ( mayaVertFilmOffset * renderSettings.width * combinedScale ) /
                         ( (float)mayaCamera.horizontalFilmAperture() * (float)mayaCamera.lensSqueezeRatio() );
        if( aspectDifference > 0.0f ) {
            // Film Fit Offset
            float vOffsetData = renderSettings.height * combinedScale /
                                ( ( (float)mayaCamera.aspectRatio() * (float)mayaCamera.lensSqueezeRatio() * 2.0f ) /
                                  aspectDifference );
            verticalOffset -= vOffsetData * (float)mayaCamera.filmFitOffset();
        }

    } else if( mayaCamera.filmFit() == MFnCamera::kVerticalFilmFit ) {
        // if the aspect ratios of the image and the resolution gate are different the image extends horizontally beyond
        // the film gate and can be cropped
        float aspectRatio = (float)mayaCamera.aspectRatio() / renderAspectRatio;

        // we have to manually calculate the perspective field of view since it is based off of the vertical film
        // aperature instead of horizontal and retrieving the values will put the values out when there is a different
        // aspect ratio
        perspective =
            2.0f * std::atan( (float)mayaCamera.verticalFilmAperture() * totalPerspectiveScale * renderAspectRatio );

        // Film Offset values - film offset is measured in Inches of film compared to the cameras aperture
        horizontalOffset =
            ( mayaHorizFilmOffset * renderSettings.height * (float)mayaCamera.lensSqueezeRatio() * combinedScale ) /
            (float)mayaCamera.verticalFilmAperture();
        verticalOffset =
            ( mayaVertFilmOffset * renderSettings.height * combinedScale ) / (float)mayaCamera.verticalFilmAperture();
        if( aspectDifference < 0.0f ) {
            // Film Fit Offset - uses a normalized unit from 1 to -1 based off the render height
            horizontalOffset +=
                ( (float)mayaCamera.filmFitOffset() * renderSettings.height * aspectDifference * combinedScale ) / 2.0f;
        }

    } else if( mayaCamera.filmFit() == MFnCamera::kFillFilmFit ) {
        // if the aspect ratios of the image and film gate are different the image extends beyond the film
        // gate/resolution gate and is cropped
        if( aspectDifference > 0.0f ) {
            float aspectRatio = (float)mayaCamera.aspectRatio() / renderAspectRatio;
            perspective = 2.0f * std::atan( ( (float)mayaCamera.horizontalFilmAperture() * totalPerspectiveScale ) /
                                            aspectRatio );

            // Film Offset values - film offset is measured in Inches of film compared to the cameras aperture
            horizontalOffset =
                ( mayaHorizFilmOffset * (float)mayaCamera.lensSqueezeRatio() * renderSettings.width * combinedScale ) /
                ( (float)mayaCamera.horizontalFilmAperture() * renderAspectRatio / (float)mayaCamera.aspectRatio() );
            verticalOffset = ( mayaVertFilmOffset * renderSettings.height * combinedScale ) /
                             ( (float)mayaCamera.verticalFilmAperture() );
        } else {
            perspective = 2.0f * std::atan( (float)mayaCamera.horizontalFilmAperture() * totalPerspectiveScale *
                                            (float)mayaCamera.lensSqueezeRatio() );

            // Film Offset values - film offset is measured in Inches of film compared to the cameras aperture
            horizontalOffset = ( mayaHorizFilmOffset * renderSettings.width * combinedScale ) /
                               (float)mayaCamera.horizontalFilmAperture();
            verticalOffset = ( mayaVertFilmOffset * renderSettings.width * combinedScale ) /
                             ( (float)mayaCamera.horizontalFilmAperture() * (float)mayaCamera.lensSqueezeRatio() );
        }
    } else if( mayaCamera.filmFit() == MFnCamera::kOverscanFilmFit ) {
        // Fits the film gate within the resolution gate. (no cropping)
        float aspectRatio = (float)mayaCamera.aspectRatio() / renderAspectRatio;

        if( aspectDifference < 0.0f ) {
            perspective = 2.0f * std::atan( ( (float)mayaCamera.horizontalFilmAperture() * totalPerspectiveScale ) /
                                            aspectRatio );
            // Film Offset values - film offset is measured in Inches of film compared to the cameras aperture
            horizontalOffset =
                mayaHorizFilmOffset * renderSettings.height * combinedScale / (float)mayaCamera.verticalFilmAperture();
            verticalOffset =
                mayaVertFilmOffset * renderSettings.height * combinedScale / (float)mayaCamera.verticalFilmAperture();
        } else {
            perspective = 2.0f * std::atan( (float)mayaCamera.horizontalFilmAperture() * totalPerspectiveScale *
                                            (float)mayaCamera.lensSqueezeRatio() );
            // Film Offset values - film offset is measured in Inches of film compared to the cameras aperture
            horizontalOffset = mayaHorizFilmOffset * renderSettings.height * combinedScale /
                               ( (float)mayaCamera.verticalFilmAperture() * aspectRatio );
            verticalOffset =
                mayaVertFilmOffset * renderSettings.height * combinedScale /
                ( (float)mayaCamera.verticalFilmAperture() * aspectRatio * (float)mayaCamera.lensSqueezeRatio() );
        }
    }

    // Film Translate - uses a normalized value based off of the width of the final render
    horizontalOffset +=
        renderSettings.width * (float)mayaCamera.filmTranslateH() * (float)mayaCamera.postScale() / 2.0f;
    verticalOffset += renderSettings.width * (float)mayaCamera.filmTranslateV() * (float)mayaCamera.postScale() / 2.0f;

    krakRenderer.set_camera_perspective_fov( perspective );
    krakRenderer.set_screen_offset( -horizontalOffset,
                                    -verticalOffset ); // screen offset works inverted in krakatoa vs maya.

    float sampleRate;
    if( mayaCamera.isDepthOfField() && get_depth_of_field( krakatoaSettingsNode, currentContext, sampleRate ) ) {
        krakRenderer.enable_depth_of_field( true );

        float mayaFstop = (float)mayaCamera.fStop();
        float mayaFocusLength = (float)mayaCamera.focalLength();
        float mayaFocusDistance = (float)mayaCamera.focusDistance();

        krakRenderer.set_depth_of_field( mayaFstop, mayaFocusLength, mayaFocusDistance, sampleRate );

    } else {
        krakRenderer.enable_depth_of_field( false );
    }

    float shutterBegin;
    float shutterEnd;
    if( get_shutter_range( krakatoaSettingsNode, currentContext, shutterBegin, shutterEnd ) ) {
        const int motionBlurParticleSegments =
            frantic::maya::get_int_attribute( krakatoaSettingsNode, "motionBlurParticleSegments", currentContext );
        const bool jitteredMotionBlur =
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "jitteredMotionBlur", currentContext );
        // is this number supposed to be the number of motion blur particle segments?  Bobo's export code just always
        // used 10 samples
        const int numMotionBlurSamples = 10;
        krakatoasr::animated_transform cameraTransform;
        if( frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "disableCameraBlur", currentContext ) ) {
            // build_static_transform( camDagPath, currentContext, cameraTransform ); // SBD: Same as below, but (0, 0)
            // instead of (middle, middle)
            const float shutterMiddle = ( shutterBegin + shutterEnd ) / 2.f;
            build_motion_transform( camDagPath, currentContext, shutterMiddle, shutterMiddle, 1, cameraTransform );
        } else {
            build_motion_transform( camDagPath, currentContext, shutterBegin, shutterEnd, numMotionBlurSamples,
                                    cameraTransform );
        }
        krakRenderer.set_camera_tm( cameraTransform );
        krakRenderer.enable_motion_blur( true );
        krakRenderer.set_motion_blur( shutterBegin, shutterEnd, motionBlurParticleSegments, jitteredMotionBlur );

        const bool enableAdaptiveMotionBlur = static_cast<bool>(
            frantic::maya::get_int_attribute( krakatoaSettingsNode, "enableAdaptiveMotionBlur", currentContext ) );
        const int adaptiveMotionBlurMinSamples =
            frantic::maya::get_int_attribute( krakatoaSettingsNode, "adaptiveMotionBlurMinSamples", currentContext );
        const int adaptiveMotionBlurMaxSamples =
            frantic::maya::get_int_attribute( krakatoaSettingsNode, "adaptiveMotionBlurMaxSamples", currentContext );
        const float adaptiveMotionBlurSmoothness =
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "adaptiveMotionBlurSmoothness", currentContext );
        const float adaptiveMotionBlurExponent =
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "adaptiveMotionBlurExponent", currentContext );

        krakRenderer.enable_adaptive_motion_blur( enableAdaptiveMotionBlur );
        if( enableAdaptiveMotionBlur ) {
            krakRenderer.set_adaptive_motion_blur_min_samples( adaptiveMotionBlurMinSamples );
            krakRenderer.set_adaptive_motion_blur_max_samples( adaptiveMotionBlurMaxSamples );
            krakRenderer.set_adaptive_motion_blur_smoothness( adaptiveMotionBlurSmoothness );
            krakRenderer.set_adaptive_motion_blur_exponent( adaptiveMotionBlurExponent );
        }
    } else {
        krakatoasr::animated_transform cameraTransform;
        build_static_transform( camDagPath, currentContext, cameraTransform );
        krakRenderer.set_camera_tm( cameraTransform );
        krakRenderer.enable_motion_blur( false );
    }

    FF_LOG( debug ) << "Krakatoa MY: Using Camera \"" << frantic::strings::to_tstring( mayaCamera.name().asChar() )
                    << "\"" << std::endl;
}

void apply_light_to_renderer( const MFnLight& mayaLight, const MFnDependencyNode& krakatoaSettingsNode,
                              const MDGContext& currentContext, krakatoasr::krakatoa_renderer& krakRenderer ) {
    krakatoasr::light* currentLight;
    krakatoasr::point_light pointLight;
    krakatoasr::spot_light spotLight;
    krakatoasr::direct_light directionalLight;

    bool useDepthMapShadows = frantic::maya::get_boolean_attribute( mayaLight, "useDepthMapShadows", currentContext );
    bool autoFocusEnabled = frantic::maya::get_boolean_attribute( mayaLight, "useDmapAutoFocus", currentContext );
    MDagPath parentNodePath = mayaLight.dagPath();
    parentNodePath.pop();
    MFnDependencyNode visibilityNode( parentNodePath.node() );

    bool visible = frantic::maya::get_boolean_attribute( visibilityNode, "visibility", currentContext ) &&
                   frantic::maya::get_boolean_attribute( mayaLight, "visibility", currentContext ) &&
                   MRenderUtil::inCurrentRenderLayer( mayaLight.dagPath() );
    if( visible ) {

        if( useDepthMapShadows && autoFocusEnabled ) {
            std::ostringstream os;
            os << "Krakatoa MY: Warning: Shadow map Auto-Focus is set for light \"" << mayaLight.name()
               << "\", however Auto-Focus is not supported in Krakatoa MY.  This option will be ignored." << std::endl;
            FF_LOG( warning ) << frantic::strings::to_tstring( os.str() );
        }

        if( mayaLight.object().hasFn( MFn::kPointLight ) ) {

            if( useDepthMapShadows ) {
                float focusAngle =
                    (float)frantic::maya::get_angle_attribute( mayaLight, "dmapFocus", currentContext ).asDegrees();

                if( !autoFocusEnabled && fabs( focusAngle - 90.0f ) > 0.000001f ) {
                    std::ostringstream os;
                    os << "Krakatoa MY: Warning: \"" << mayaLight.name()
                       << "\" has shadow map focus angle set to a value other than 90.0.  Krakatoa MY does not angles "
                          "other than 90.0 for point lights."
                       << std::endl;
                    FF_LOG( warning ) << frantic::strings::to_tstring( os.str() );
                }
            }
            currentLight = &pointLight;
        } else if( mayaLight.object().hasFn( MFn::kDirectionalLight ) ) {
            if( !mayaLight.hasAttribute( "krakatoaLightRadius" ) ) {
                float mayaWidthFocus =
                    frantic::maya::get_float_attribute( mayaLight, "dmapWidthFocus", currentContext );
                MString command =
                    MString( "addAttr -longName \"krakatoaLightRadius\" -attributeType \"float\" -min 0.5 -dv " ) +
                    boost::lexical_cast<std::string>( mayaWidthFocus ).c_str() + " " + mayaLight.name() + ";";
                MGlobal::executeCommand( command );
                FF_LOG( stats ) << "Added attribute \"krakatoaLightRadius\" to directional light "
                                << mayaLight.name().asChar() << std::endl;
            }

            if( !mayaLight.hasAttribute( "krakatoaPenumbraRadius" ) ) {
                float mayaWidthFocus =
                    frantic::maya::get_float_attribute( mayaLight, "dmapWidthFocus", currentContext );
                MString command =
                    MString( "addAttr -longName \"krakatoaPenumbraRadius\" -attributeType \"float\"  -dv 0.0 " +
                             mayaLight.name() + ";" );
                MGlobal::executeCommand( command );
                FF_LOG( stats ) << "Added attribute \"krakatoaPenumbraRadius\" to directional light "
                                << mayaLight.name().asChar() << std::endl;
            }

            if( !mayaLight.hasAttribute( "krakatoaDirectionalShape" ) ) {
                MString command =
                    MString( "addAttr -longName \"krakatoaDirectionalShape\" -at enum -enumName \"Round:Square\" " +
                             mayaLight.name() + ";" );
                MGlobal::executeCommand( command );
                FF_LOG( stats ) << "Added attribute \"krakatoaDirectionalShape\" to directional light "
                                << mayaLight.name().asChar() << std::endl;
            }

            MString shape = frantic::maya::get_enum_attribute( mayaLight, "krakatoaDirectionalShape", currentContext );
            if( shape == "Round" ) {
                directionalLight.set_light_shape( krakatoasr::SHAPE_ROUND );
            } else {
                directionalLight.set_light_shape( krakatoasr::SHAPE_SQUARE );
            }

            float width = frantic::maya::get_float_attribute( mayaLight, "krakatoaLightRadius", currentContext );
            float widthWithPenumbra =
                width + frantic::maya::get_float_attribute( mayaLight, "krakatoaPenumbraRadius", currentContext );

            if( width <= widthWithPenumbra ) {
                directionalLight.set_rect_radius( width, widthWithPenumbra );
            } else {
                directionalLight.set_rect_radius( widthWithPenumbra, width );
            }
            currentLight = &directionalLight;
        } else if( mayaLight.object().hasFn( MFn::kSpotLight ) ) {

            float coneAngle =
                (float)frantic::maya::get_angle_attribute( mayaLight, "coneAngle", currentContext ).asDegrees();
            float penumbraAngle =
                (float)frantic::maya::get_angle_attribute( mayaLight, "penumbraAngle", currentContext ).asDegrees();

            float minAngle;
            float maxAngle;

            if( penumbraAngle >= 0 ) {
                minAngle = coneAngle;
                maxAngle = coneAngle + penumbraAngle;
            } else {
                minAngle = coneAngle + penumbraAngle;
                maxAngle = coneAngle;
            }

            spotLight.set_cone_angle( minAngle, maxAngle );

            // there seems to be an option in maya to create a rectangular light by applying a 'barnDoors' attribute,
            // but it looks like we don't really support that kind of effect
            spotLight.set_light_shape( krakatoasr::SHAPE_ROUND );
            currentLight = &spotLight;

            if( useDepthMapShadows ) {
                float focusAngle =
                    (float)frantic::maya::get_angle_attribute( mayaLight, "dmapFocus", currentContext ).asDegrees();

                if( !autoFocusEnabled && fabsf( focusAngle - maxAngle ) > 0.000001f ) {
                    std::ostringstream os;
                    os << "Krakatoa MY: Warning: \"" << mayaLight.name()
                       << "\" has shadow map focus angle set to a value differing from its cone angle.  Krakatoa MY "
                          "does not support differing cone angles for the shadow map focus angle.  The angle "
                       << maxAngle << " will be used." << std::endl;
                    FF_LOG( warning ) << frantic::strings::to_tstring( os.str() );
                }
            }

        } else {
            FF_LOG( warning ) << "The light \"" << frantic::strings::to_tstring( mayaLight.name().asChar() )
                              << "\" could not be applied because that type of light is not currently supported."
                              << std::endl;
            return;
        }

        const color3f lightColor = frantic::maya::get_color_attribute( mayaLight, "color", currentContext );
        const float intensity = frantic::maya::get_float_attribute( mayaLight, "intensity", currentContext );
        const int decayRate = frantic::maya::get_int_attribute( mayaLight, "decayRate", currentContext );
        const float multiplier = static_cast<float>( intensity * 4 * M_PI );
        currentLight->set_flux( lightColor.r * multiplier, lightColor.g * multiplier, lightColor.b * multiplier );
        currentLight->set_decay_exponent( decayRate );

        currentLight->enable_shadow_map( useDepthMapShadows );
        if( useDepthMapShadows ) {
            currentLight->set_shadow_density( 1.0f );
            currentLight->set_shadow_map_width(
                (int)frantic::maya::get_float_attribute( mayaLight, "dmapResolution", currentContext ) );
        }

        krakatoasr::animated_transform lightTransform;
        MDagPath lightPath;
        mayaLight.getPath( lightPath );
        float shutterBegin;
        float shutterEnd;

        MFnDependencyNode depNode( lightPath.node() );
        if( get_shutter_range( krakatoaSettingsNode, currentContext, shutterBegin, shutterEnd ) ) {
            const int numMotionBlurSamples = 10;
            build_motion_transform( lightPath, currentContext, shutterBegin, shutterEnd, numMotionBlurSamples,
                                    lightTransform );
        } else {
            build_static_transform( lightPath, currentContext, lightTransform );
        }

        currentLight->set_name( mayaLight.name().asChar() );
        krakRenderer.add_light( currentLight, lightTransform );
        FF_LOG( debug ) << "Krakatoa MY: Got Light \"" << frantic::strings::to_tstring( mayaLight.name().asChar() )
                        << "\"" << std::endl;
    }
}

void apply_prt_object_to_renderer( const MFnDagNode& fnPathNode, const MFnDependencyNode& prtNode,
                                   const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                   krakatoasr::krakatoa_renderer& krakRenderer ) {
    krakatoasr::animated_transform objectTransform;
    krakatoasr::particle_stream streamData;

    MDagPath prtObjectPath;
    fnPathNode.getPath( prtObjectPath );

    MObject endOfChain = PRTObjectBase::getEndOfStreamChain( prtNode );
    MFnDependencyNode depNode( endOfChain );

    build_scene_transform( prtObjectPath, krakatoaSettingsNode, currentContext, objectTransform );

    frantic::graphics::transform4f tm;
    objectTransform.get_transform( (float*)( &tm ) );
    streamData.get_data()->stream = PRTObjectBase::getParticleStreamFromMPxData( depNode, tm, currentContext, false );

    // create a deep copy so we can pop the top node off of one
    MDagPath parentPath( prtObjectPath );
    parentPath.pop();
    MFnDependencyNode visibilityNode( parentPath.node() );

    bool visible = frantic::maya::get_boolean_attribute( visibilityNode, "visibility", currentContext ) &&
                   MRenderUtil::inCurrentRenderLayer( prtObjectPath );

    if( visible ) {
        streamData.get_data()->tm = objectTransform;

        krakatoasr::cancel_render_interface* cancelCheck = krakRenderer.get_params()->cancelRenderCheck;
        krakatoasr::progress_logger_interface* progressLogger = krakRenderer.get_params()->progressLoggerUpdater;
        apply_common_operations_to_stream( krakatoaSettingsNode, currentContext, streamData );

        krakRenderer.add_particle_stream( streamData );
        FF_LOG( debug ) << "Krakatoa MY: Got PRT Object Node \""
                        << frantic::strings::to_tstring( fnPathNode.name().asChar() ) << "\"" << std::endl;
    }
}

void apply_maya_mesh_to_renderer( const MDagPath& meshDagPath, const MFnDependencyNode& krakatoaSettingsNode,
                                  const MDGContext& currentContext, krakatoasr::krakatoa_renderer& krakRenderer,
                                  mesh_context& sharedMeshContext ) {
    MFnMesh fnMesh( meshDagPath );

    MDagPath parentPath;
    fnMesh.getPath( parentPath );
    parentPath.pop();
    MFnDependencyNode visibilityNode( parentPath.node() );

    bool visible = frantic::maya::get_boolean_attribute( fnMesh, "visibility", currentContext ) &&
                   frantic::maya::get_boolean_attribute( visibilityNode, "visibility", currentContext ) &&
                   MRenderUtil::inCurrentRenderLayer( meshDagPath );

    // intermediate objects should not be seen
    bool isIntermediateObject = frantic::maya::get_boolean_attribute( fnMesh, "intermediateObject", currentContext );

    if( visible && !isIntermediateObject ) {

        // Check to see if the user has tagged this mesh as a matte object.
        bool isTaggedAsMatte = has_krakatoa_matte_tag( fnMesh, currentContext );
        if( isTaggedAsMatte ) {

            bool meshCastsShadows = frantic::maya::get_boolean_attribute( fnMesh, "castsShadows", currentContext );
            bool meshIsCameraVisible =
                frantic::maya::get_boolean_attribute( fnMesh, "primaryVisibility", currentContext );

            // can be overridden by render layers
            int castShadowOverride = get_render_layer_setting( "castsShadows", currentContext );
            if( castShadowOverride != -1 )
                meshCastsShadows = castShadowOverride != 0;
            int cameraVisibleOverride = get_render_layer_setting( "primaryVisibility", currentContext );
            if( cameraVisibleOverride )
                meshIsCameraVisible = cameraVisibleOverride != 0;

            if( meshCastsShadows || meshIsCameraVisible ) {
                frantic::tstring meshKey;

                // create a key for this mesh to be entered into our mesh_context.
                // mesh context is needed until the end of the render because meshes must stay in memory
                // and this way, we can reuse instanced meshes.
                bool hasKeyedAttribute = trace_input_mesh_plug( fnMesh, _T( "inMesh" ), meshKey );
                if( !hasKeyedAttribute )
                    meshKey = frantic::maya::from_maya_t( meshDagPath.fullPathName() ) + _T( ".inMesh" );

                // This will try to key the meshes based on their source plug's name.  Hence, if multiple 'mesh' objects
                // all received their input from the same mesh (instanced meshes), this will only generate 1 copy of the
                // geometry, and share it among all of the meshes in the scene

                // TODO: CONRAD, don't remake the mesh if it exists!
                krakatoasr::triangle_mesh* ksrMesh = sharedMeshContext.get_shared_instance( meshKey );
                trimesh3& internalMesh = *ksrMesh->get_data()->mesh;

                MStatus status;
                MFnDagNode fnDagNode( meshDagPath );
                MPlug meshPlug = fnDagNode.findPlug( "outMesh", &status );
                if( !status )
                    throw std::runtime_error( "Could not get \"outMesh\" plug from DAG path for mesh \"" +
                                              std::string( fnMesh.name().asChar() ) + "\"" );

                // Determine in motion blur is enabled.
                float shutterBegin;
                float shutterEnd;
                bool useMotionBlur =
                    get_shutter_range( krakatoaSettingsNode, currentContext, shutterBegin, shutterEnd );

                // Copy the mesh from Maya into our own mesh class.
                frantic::maya::geometry::copy_maya_mesh( meshPlug, internalMesh, true, false, useMotionBlur, false,
                                                         true );

                // We need to init the velocity accessor. Normally it's done internally, but we're subverting the API's
                // normal mesh building process.
                if( internalMesh.has_vertex_channel( _T( "Velocity" ) ) )
                    ksrMesh->get_data()->velocityAcc =
                        internalMesh.get_vertex_channel_accessor<vector3f>( _T( "Velocity" ) );

                // Set the visible flags.
                ksrMesh->set_visible_to_camera( meshIsCameraVisible );
                ksrMesh->set_visible_to_lights( meshCastsShadows );

                // Create the object transformation matrix.
                krakatoasr::animated_transform objectTransform;
                build_scene_transform( meshDagPath, krakatoaSettingsNode, currentContext, objectTransform );

                // Add the mesh to the renderer.
                krakRenderer.add_mesh( ksrMesh, objectTransform );

                FF_LOG( debug ) << "Krakatoa MY: Got Maya Matte Mesh \""
                                << frantic::strings::to_tstring( fnMesh.name().asChar() ) << "\"" << std::endl;
            }
        }
    }
}

/**
 * Applies general 'copy','set' and 'fractional' channel operations to an incomming particle stream, before it is then
 * applied to the render
 */
void apply_common_operations_to_stream( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                        krakatoasr::particle_stream& particleStream ) {
    if( frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "overrideColor", currentContext ) ) {
        frantic::graphics::color3f colorOverride =
            frantic::maya::get_color_attribute( krakatoaSettingsNode, "colorChannelOverride", currentContext );
        krakatoasr::channelop_set_vector(
            particleStream, frantic::strings::to_string( frantic::maya::particles::PRTColorChannelName ).c_str(),
            colorOverride.r, colorOverride.g, colorOverride.b );
    }

    if( frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "overrideEmission", currentContext ) ) {
        frantic::graphics::color3f emissionOverride =
            frantic::maya::get_color_attribute( krakatoaSettingsNode, "emissionChannelOverride", currentContext );
        krakatoasr::channelop_set_vector(
            particleStream, frantic::strings::to_string( frantic::maya::particles::PRTEmissionChannelName ).c_str(),
            emissionOverride.r, emissionOverride.g, emissionOverride.b );
    }

    if( frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "overrideAbsorption", currentContext ) ) {
        frantic::graphics::color3f absorptionOverride =
            frantic::maya::get_color_attribute( krakatoaSettingsNode, "absorptionChannelOverride", currentContext );
        krakatoasr::channelop_set_vector(
            particleStream, frantic::strings::to_string( frantic::maya::particles::PRTAbsorptionChannelName ).c_str(),
            absorptionOverride.r, absorptionOverride.g, absorptionOverride.b );
    }

    if( frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "overrideDensity", currentContext ) ) {
        float densityOverride =
            frantic::maya::get_float_attribute( krakatoaSettingsNode, "densityChannelOverride", currentContext );
        krakatoasr::channelop_set_float(
            particleStream, frantic::strings::to_string( frantic::maya::particles::PRTDensityChannelName ).c_str(),
            densityOverride );
    }

    // apply global fractional particle stream.
    const float fraction =
        frantic::maya::get_float_attribute( krakatoaSettingsNode, "loadPercentage", currentContext ) / 100.f;
    krakatoasr::add_fractional_particle_stream( particleStream, fraction );
}

/**
 * Checks if this object has been tagged as a 'KrakatoaMatte' object, i.e. the attribute 'KrakatoaMatte' has been added,
 * and is set to 'true'
 */
bool has_krakatoa_matte_tag( const MFnDependencyNode& fnNode, const MDGContext& currentContext ) {
    MStatus outStatus;
    bool krakatoaMatteAttrib =
        frantic::maya::get_boolean_attribute( fnNode, "KrakatoaMatte", currentContext, &outStatus );
    if( !outStatus )
        krakatoaMatteAttrib = false;
    return krakatoaMatteAttrib;
}

/**
 * Builds an animated_transform by taking 'numSamples' in the range between 'shutterBegin' and 'shutterEnd', centered
 * around 'currentTime'.
 */
void build_motion_transform( const MDagPath& dagNodePath, const MDGContext& currentContext, float shutterBegin,
                             float shutterEnd, int numSamples, krakatoasr::animated_transform& outTForm ) {
    MTime currentTime;
    currentContext.getTime( currentTime );

    for( int i = 0; i < numSamples; ++i ) {
        const float currentShutterTime = shutter_angle_iterator( shutterBegin, shutterEnd, i, numSamples );
        MTime iterationTime = ( currentTime + MTime( currentShutterTime, MTime::kSeconds ) );

        transform4f franticMatrix;
        maya_util::get_object_world_matrix( dagNodePath, MDGContext( iterationTime ), franticMatrix );

        outTForm.add_transform( &franticMatrix[0], currentShutterTime );
    }
}

/**
 * Builds an animated_transform containing only a single sample taken at 'currentTime'
 */
void build_static_transform( const MDagPath& dagNodePath, const MDGContext& currentContext,
                             krakatoasr::animated_transform& outTForm ) {
    // this is the same as taking a single sample exactly at 'currentTime'
    build_motion_transform( dagNodePath, currentContext, 0.0f, 0.0f, 1, outTForm );
}

/**
 * Builds an appropriate animated_transform based on the krakatoa render parameters
 */
void build_scene_transform( const MDagPath& dagNodePath, const MFnDependencyNode& krakatoaSettingsNode,
                            const MDGContext& currentContext, krakatoasr::animated_transform& outTForm ) {
    float shutterBegin;
    float shutterEnd;

    if( get_shutter_range( krakatoaSettingsNode, currentContext, shutterBegin, shutterEnd ) ) {
        const int numMotionBlurSamples = 10;
        maya_ksr::build_motion_transform( dagNodePath, currentContext, shutterBegin, shutterEnd, numMotionBlurSamples,
                                          outTForm );
    } else {
        maya_ksr::build_static_transform( dagNodePath, currentContext, outTForm );
    }
}

boost::shared_ptr<particle_istream> get_renderer_stream_modifications( boost::shared_ptr<particle_istream> inStream,
                                                                       const MDGContext& currentContext ) {
    // TODO: DELETE?

    // This is a utility function to set up a particle stream to match what the render looks like.

    // the reason this function exists is a little hacky.
    // it was needed because all the stream setup logic was in the render code.
    // however, we needed that same logic for things like: displaying particles in the viewport, saving particles to
    // disk. so this utility function was made.

    krakatoasr::particle_stream streamWrap;
    streamWrap.get_data()->stream = inStream;

    MObject krakatoaSettingsNodeObj;
    bool found = maya_util::find_node( KrakatoaSettingsNodeName::nodeName, krakatoaSettingsNodeObj );

    if( found ) {
        MFnDependencyNode krakatoaSettingsNode( krakatoaSettingsNodeObj );
        // this function will apply the modifiers, and the global settings overrides and streams.
        maya_ksr::apply_common_operations_to_stream( krakatoaSettingsNode, currentContext, streamWrap );
    }

    return streamWrap.get_data()->stream;
}

int get_render_layer_setting( const std::string& settingName, const MDGContext& currentContext ) {
    MFnRenderLayer layer( MFnRenderLayer::currentLayer() );
    std::string overridesString =
        frantic::maya::get_string_attribute( layer, settingName.c_str(), currentContext ).asChar();
    // this is the format of the string:
    //"castsShadows=0 receiveShadows=0 motionBlur=0 primaryVisibility=0"
    if( !overridesString.empty() ) {
        boost::regex reg( "\\s" );
        std::string noWhitespaceStr = boost::regex_replace( overridesString, reg, "" ); // remove all whitespace
        size_t pos = noWhitespaceStr.find( settingName ); // find the spot where there our setting lies
        if( pos != std::string::npos )
            return boost::lexical_cast<int>(
                noWhitespaceStr.substr( pos + settingName.length() + 1, 1 ) ); // get the charater after the "="
    }
    return -1;
}

boost::shared_array<krakatoasr::frame_buffer_pixel_data>
evaluate_texture( int textureWidth, const MString& attributeName, const MFnDependencyNode& krakatoaSettingsNode,
                  const MDGContext& currentContext ) {
    const int numSamples = textureWidth * textureWidth;

    boost::shared_array<krakatoasr::frame_buffer_pixel_data> buffer(
        new krakatoasr::frame_buffer_pixel_data[numSamples] );
    MPlug colorPlug = krakatoaSettingsNode.findPlug( attributeName );

    if( !colorPlug.isConnected() ) {
        color3f kModColor = frantic::maya::get_color_attribute( krakatoaSettingsNode, attributeName, currentContext );
        int index = 0;
        for( int u = 0; u < textureWidth; ++u ) {
            for( int v = 0; v < textureWidth; ++v, ++index ) {
                krakatoasr::frame_buffer_pixel_data currColor;
                currColor.r = kModColor.r;
                currColor.g = kModColor.g;
                currColor.b = kModColor.b;
                currColor.r_alpha = 1.0f;
                currColor.g_alpha = 1.0f;
                currColor.b_alpha = 1.0f;
                buffer[index] = currColor;
            }
        }
    } else {
        MPlugArray colorPlugConnections;
        colorPlug.connectedTo( colorPlugConnections, true, false );
        if( colorPlugConnections.length() >= 1 ) {
            MFnDependencyNode connectionNode( colorPlugConnections[0].node() );
            std::string textureNodeName = connectionNode.name().asChar();
            frantic::maya::particles::maya_texture_type_t textureType =
                frantic::maya::particles::get_texture_type( textureNodeName );
            if( textureType != frantic::maya::particles::TEXTURE_TYPE_2D ) {
                throw std::runtime_error( "Krakatoa Error: Bokeh maps only support 2D textures." );
            }

            MFloatVectorArray textureEvalColors;
            MFloatVectorArray textureEvalAlphas;

            MFloatMatrix cameraMatrix;
            cameraMatrix.setToIdentity();

            MFloatArray uCoords;
            MFloatArray vCoords;
            uCoords.clear();
            vCoords.clear();

            for( int u = 0; u < textureWidth; ++u ) {
                for( int v = 0; v < textureWidth; ++v ) {
                    uCoords.append( static_cast<float>( u ) / static_cast<float>( textureWidth ) );
                    vCoords.append( static_cast<float>( v ) / static_cast<float>( textureWidth ) );
                }
            }

            MRenderUtil::sampleShadingNetwork(
                frantic::maya::to_maya_t( frantic::strings::to_tstring( textureNodeName ) + _T( ".outColor" ) ),
                numSamples, false, false, cameraMatrix, NULL, &uCoords, &vCoords, NULL, NULL, NULL, NULL, NULL,
                textureEvalColors, textureEvalAlphas );

            int index = 0;
            for( int u = 0; u < textureWidth; ++u ) {
                for( int v = 0; v < textureWidth; ++v, ++index ) {
                    krakatoasr::frame_buffer_pixel_data currColor;

                    currColor.r = textureEvalColors[index].x;
                    currColor.g = textureEvalColors[index].y;
                    currColor.b = textureEvalColors[index].z;

                    currColor.r_alpha = 1.0f - textureEvalAlphas[index].x;
                    currColor.g_alpha = 1.0f - textureEvalAlphas[index].y;
                    currColor.b_alpha = 1.0f - textureEvalAlphas[index].z;

                    buffer[index] = currColor;
                }
            }
        }
    }

    return buffer;
}

bokeh_map_ptrs apply_bokeh_settings_to_renderer( const MFnCamera& mayaCamera,
                                                 const MFnDependencyNode& krakatoaSettingsNode,
                                                 const MDGContext& currentContext,
                                                 krakatoasr::krakatoa_renderer& krakRenderer ) {

    bokeh_map_ptrs buffers;

    if( mayaCamera.isDepthOfField() &&
        frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "enableDOF", currentContext ) ) {
        const bool useBokehShapeMap =
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "enableBokehShapeMap", currentContext );
        const bool useBokehBlendMap =
            frantic::maya::get_boolean_attribute( krakatoaSettingsNode, "enableBokehBlendMap", currentContext );
        const bool useBokehAnamorphicSqueeze = frantic::maya::get_boolean_attribute(
            krakatoaSettingsNode, "enableBokehAnamorphicSqueeze", currentContext );

        static const int textureWidth = 256;

        if( useBokehShapeMap ) {
            boost::shared_array<krakatoasr::frame_buffer_pixel_data> texture =
                evaluate_texture( textureWidth, "bokehShapeMap", krakatoaSettingsNode, currentContext );
            krakatoasr::texture_data textureData( textureWidth, texture.get() );
            krakRenderer.set_bokeh_shape_map( textureData );
            buffers.shapeMapPtr = texture;
        }

        if( useBokehBlendMap ) {
            boost::shared_array<krakatoasr::frame_buffer_pixel_data> texture =
                evaluate_texture( textureWidth, "bokehBlendMap", krakatoaSettingsNode, currentContext );
            krakatoasr::texture_data textureData( textureWidth, texture.get() );
            krakRenderer.set_bokeh_blend_map( textureData );
            buffers.blendMapPtr = texture;

            const float bokehBlendInfluence =
                frantic::maya::get_float_attribute( krakatoaSettingsNode, "bokehBlendInfluence", currentContext );
            krakRenderer.set_bokeh_blend_influence( bokehBlendInfluence );

            const int bokehBlendMipmapScale =
                frantic::maya::get_int_attribute( krakatoaSettingsNode, "bokehBlendMipmapScale", currentContext );
            const bool allocateBokehBlendInfluence = frantic::maya::get_boolean_attribute(
                krakatoaSettingsNode, "allocateBokehBlendInfluence", currentContext );

            krakRenderer.set_bokeh_blend_mipmap_scale( bokehBlendMipmapScale );
            krakRenderer.set_allocate_bokeh_blend_influence( allocateBokehBlendInfluence );
        }

        if( useBokehAnamorphicSqueeze ) {
            const float anamorphicSqueeze =
                frantic::maya::get_float_attribute( krakatoaSettingsNode, "bokehAnamorphicSqueeze", currentContext );
            krakRenderer.set_anamorphic_squeeze( anamorphicSqueeze );
        }
    }

    return buffers;
}

bokeh_map_ptrs apply_bokeh_settings_to_renderer_with_current_camera( const MFnDependencyNode& krakatoaSettingsNode,
                                                                     const MDGContext& currentContext,
                                                                     krakatoasr::krakatoa_renderer& krakRenderer ) {
    MStatus status;
    M3dView currentView = M3dView::active3dView( &status );

    if( !status ) {
        FF_LOG( stats ) << "Krakatoa MY: Could not get an active 3d view!\n";
        return bokeh_map_ptrs();
    }

    MDagPath camDagPath;
    currentView.getCamera( camDagPath );
    MFnCamera mayaCamera( camDagPath, &status );
    if( !status ) {
        FF_LOG( stats ) << "Krakatoa MY: Could not get the current render camera!\n";
        return bokeh_map_ptrs();
    }

    return apply_bokeh_settings_to_renderer( mayaCamera, krakatoaSettingsNode, currentContext, krakRenderer );
}

bokeh_map_ptrs apply_bokeh_settings_to_renderer_with_chosen_camera( const MFnDependencyNode& krakatoaSettingsNode,
                                                                    const MDGContext& currentContext,
                                                                    krakatoasr::krakatoa_renderer& krakRenderer,
                                                                    MString camera ) {
    MStatus status;
    MSelectionList list;
    MGlobal::getSelectionListByName( camera, list );
    MDagPath camDagPath;
    list.getDagPath( 0, camDagPath );
    MFnCamera mayaCamera( camDagPath, &status );
    if( !status ) {
        FF_LOG( stats ) << "Krakatoa MY: Could not get the current render camera!\n";
        return bokeh_map_ptrs();
    }

    return apply_bokeh_settings_to_renderer( mayaCamera, krakatoaSettingsNode, currentContext, krakRenderer );
}

} // namespace maya_ksr
