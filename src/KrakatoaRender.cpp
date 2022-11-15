// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <ImfIO.h>
#include <boost/regex.hpp>

#include "KrakatoaRender.hpp"

#include <maya/M3dView.h>
#include <maya/MAnimControl.h>
#include <maya/MCommonRenderSettingsData.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFnCamera.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDirectionalLight.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNonAmbientLight.h>
#include <maya/MFnPointLight.h>
#include <maya/MFnSpotLight.h>
#include <maya/MGlobal.h>
#include <maya/MImage.h>
#include <maya/MItDag.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MRenderUtil.h>
#include <maya/MRenderView.h>
#include <maya/MStatus.h>

#include <frantic/graphics2d/image/image_file_io.hpp>

#include <krakatoasr_renderer.hpp>
#include <krakatoasr_renderer/params.hpp>
#include <krakatoasr_renderer/renderer.hpp>

#include <exception>

#include <frantic/channels/channel_map.hpp>
#include <frantic/channels/named_channel_data.hpp>
#include <frantic/diagnostics/profiling_section.hpp>
#include <frantic/files/paths.hpp>
#include <frantic/graphics/color3f.hpp>
#include <frantic/graphics/transform4f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/graphics2d/image/image_file_io.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/shared_particle_container_particle_istream.hpp>
#include <frantic/particles/streams/transformed_particle_istream.hpp>

#include <boost/algorithm/string/replace.hpp>
#include <boost/shared_ptr.hpp>

#include <frantic/maya/attributes.hpp>
#include <frantic/maya/particles/particles.hpp>
#include <frantic/maya/util.hpp>

#include "KrakatoaSettings.hpp"
#include "PRTLoader.hpp"
#include "PRTObjectUI.hpp"
#include "maya_frame_buffer_interface.hpp"
#include "maya_ksr.hpp"
#include "maya_progress_bar_interface.hpp"
#include "maya_render_save_interface.hpp"
#include <frantic/maya/maya_util.hpp>

#include <iostream>
#include <vector>

using namespace frantic;
using namespace frantic::graphics;
using namespace frantic::particles;
using namespace frantic::particles::streams;
using namespace frantic::channels;
using namespace frantic::maya;

namespace {

void DumpArgs( const MArgList& args ) {
    std::cout << "Number of Parameters: " << args.length() << std::endl;

    for( unsigned int i = 0; i < args.length(); ++i ) {
        std::cout << "Parameter #" << i << ": str:\"" << args.asString( i ) << "\""
                  << " int:" << args.asInt( i ) << " double:" << args.asDouble( i ) << std::endl;
    }
}

frantic::tstring get_output_image_extension() {
    frantic::tstring output;
    int renderImageFormat = maya_util::get_current_render_image_format();
    if( maya_util::get_image_format_extension( renderImageFormat, output ) ) {
        return output;
    } else {
        FF_LOG( stats ) << "Image format " << renderImageFormat << " not recognized, using default (iff)" << std::endl;
        return _T("iff");
    }
}

int set_maya_tbb_thread_count( int krakThreadCount ) {
    // to properly set the thread count in maya, we must use maya's calls, because we share the same tbb dll.
    // so, first query the number of threads mayais using, then set the number. after we will reset it back to the
    // original count.
    int originalThreadCount;
    MGlobal::executeCommand( "threadCount -q -n;", originalThreadCount );
    if( krakThreadCount > 0 ) {
        std::stringstream melCmd;
        melCmd << "threadCount -n " << krakThreadCount << ";";
        MGlobal::executeCommand( melCmd.str().c_str() );
        FF_LOG( debug ) << "Original number of threads prior to render: " << originalThreadCount
                        << ". Setting new thread count to " << krakThreadCount << ".\n";
    }
    return originalThreadCount;
}
void restore_maya_tbb_thread_count( int originalThreadCount ) {
    std::stringstream melCmd;
    melCmd << "threadCount -n " << originalThreadCount << ";";
    MGlobal::executeCommand( melCmd.str().c_str() );
}

// wrapper function that sets the thread count in maya, and launches a render, then restores the original thread count
// in maya.
void launch_krakatoa_render( const MFnDependencyNode& settingsNode, MDGContext currentContext,
                             krakatoasr::krakatoa_renderer& krakRenderer ) {
    krakatoasr::krakatoa_renderer_params& renderingParams = *krakRenderer.get_params();

    int krakThreadCount = frantic::maya::get_int_attribute( settingsNode, "threadCount", currentContext );
    int originalThreadCount = set_maya_tbb_thread_count( krakThreadCount );

    try {
        bool success = krakatoasr::render_scene( renderingParams );
        if( !success ) {
            std::string message = "Krakatoa MY: Render cancelled by user.";
            MGlobal::displayWarning( message.c_str() );
            FF_LOG( stats ) << frantic::strings::to_tstring( message ) << std::endl;
        }
    } catch( std::runtime_error& ) {
        restore_maya_tbb_thread_count( originalThreadCount );
        throw;
    }
    restore_maya_tbb_thread_count( originalThreadCount );
}

} // namespace

static void setCurrentTime( MTime timeIt ) {
    // Update the particles.  For some reason, not all the properties update automatically when you change the time.
    std::vector<MDagPath> mayaParticles;
    frantic::maya::maya_util::find_nodes_with_type( MFn::kParticle, mayaParticles );
    for( std::vector<MDagPath>::const_iterator iter = mayaParticles.begin(); iter != mayaParticles.end(); ++iter ) {
        MStatus status;
        MFnParticleSystem particles( iter->node(), &status );
        if( status == MS::kSuccess )
            particles.evaluateDynamics( timeIt, false );
    }

    MAnimControl::setCurrentTime( timeIt );
}

const MString KrakatoaRender::commandName = "KrakatoaRender";

void* KrakatoaRender::creator() { return new KrakatoaRender(); }

MStatus KrakatoaRender::doIt( const MArgList& args ) {

    frantic::diagnostics::profiling_section psTotalRenderTime( _T( "Total frame render time" ) );
    psTotalRenderTime.enter();

    MStatus status;
    krakatoasr::krakatoa_renderer krakRenderer;
    maya_ksr::mesh_context meshContext;
    MObject krakatoaSettingsNodeObj;
    bool found = maya_util::find_node( KrakatoaSettingsNodeName::nodeName, krakatoaSettingsNodeObj );

    try {
        if( found ) {
            MFnDependencyNode settingsNode( krakatoaSettingsNodeObj );

            MTime currentTime = MAnimControl::currentTime();
            MDGContext currentContext( currentTime );

            // get the user-specified camera from the arguments (if it exits). if it doesn't exist, we use the current
            // viewport.
            unsigned index = args.flagIndex( "c", "cam" );
            MString cameraName = _T("");
            if( index != MArgList::kInvalidArgIndex )
                args.get( index + 1, cameraName );

            index = args.flagIndex( "l", "layer" );
            MString layer = _T("");
            if( index != MArgList::kInvalidArgIndex )
                args.get( index + 1, layer );

            if( layer != _T("") ) {
                MString cmd = "editRenderLayerGlobals -crl " + layer;
                if( !MGlobal::executeCommand( cmd ) ) {
                    FF_LOG( error ) << "Layer \"" << frantic::strings::to_tstring( layer.asChar() )
                                    << "\" does not exist.\n";
                    return MStatus::kFailure;
                }
            }

            maya_frame_buffer_interface mayaFrameBuffer;
            mayaFrameBuffer.set_camera_name( frantic::strings::to_tstring( cameraName.asChar() ) );
            krakRenderer.set_frame_buffer_update( &mayaFrameBuffer );

            maya_render_save_interface saveInterface( settingsNode, currentContext );
            krakRenderer.set_render_save_callback( &saveInterface );
            maya_progress_bar_interface mayaProgressBar;
            mayaProgressBar.set_num_frames( 1 );
            mayaProgressBar.set_current_frame( 0 );
            krakRenderer.set_progress_logger_update( &mayaProgressBar );
            krakRenderer.set_cancel_render_callback( &mayaProgressBar );

            FF_LOG( stats ) << "Krakatoa MY: Rendering Frame " << currentTime.asUnits( MTime::uiUnit() )
                            << " to file \"" << saveInterface.get_output_image_name() << "\"" << std::endl;
            mayaProgressBar.begin_display();
            // We are checking if maya wants to cancel immediately if it does we are resetting the display
            // Maya should only want to cancel immediately if a cancel request was sent after we end the display
            if( mayaProgressBar.is_cancelled() ) {
                mayaProgressBar.end_display();
                mayaProgressBar.begin_display();
            }

            maya_ksr::bokeh_map_ptrs bokehMapPtrs;

            // set up the scene camera
            maya_ksr::apply_scene_to_renderer( settingsNode, currentContext, krakRenderer, meshContext );
            if( cameraName == _T("") ) {
                maya_ksr::apply_current_camera_to_renderer( settingsNode, currentContext, krakRenderer );
                bokehMapPtrs = maya_ksr::apply_bokeh_settings_to_renderer_with_current_camera(
                    settingsNode, currentContext, krakRenderer );
            } else {
                maya_ksr::apply_chosen_camera_to_renderer( settingsNode, currentContext, krakRenderer, cameraName );
                bokehMapPtrs = maya_ksr::apply_bokeh_settings_to_renderer_with_chosen_camera(
                    settingsNode, currentContext, krakRenderer, cameraName );
            }

            // launch the rendering function
            launch_krakatoa_render( settingsNode, currentContext, krakRenderer );

            mayaProgressBar.end_display();

        } else {
            MGlobal::displayError( "Krakatoa settings node was not found, cannot render.\n" );
        }
    } catch( std::exception& e ) {
        std::ostringstream os;
        os << "Krakatoa MY: ERROR : " << e.what() << std::endl;
        FF_LOG( stats ) << os.str().c_str();
        MGlobal::displayError( os.str().c_str() );
    }

    // log total rendering time
    psTotalRenderTime.exit();
    FF_LOG( stats ) << psTotalRenderTime << std::endl;

    return MStatus::kSuccess;
}

const MString KrakatoaBatchRender::commandName = "KrakatoaBatchRender";

void* KrakatoaBatchRender::creator() { return new KrakatoaBatchRender(); }

MStatus KrakatoaBatchRender::doIt( const MArgList& args ) {
    MStatus status;
    MObject krakatoaSettingsNodeObj;
    bool found = maya_util::find_node( KrakatoaSettingsNodeName::nodeName, krakatoaSettingsNodeObj );

    try {
        if( found ) {
            MFnDependencyNode settingsNode( krakatoaSettingsNodeObj );
            MCommonRenderSettingsData renderData;
            MRenderUtil::getCommonRenderSettings( renderData );
            // If we have not been given a prefix for our render we want to use the scene name, batch renders add __####
            // to the end of the scene name though
            //  therefore we are removing the end characters and setting it to the prefix
            if( renderData.name == "" ) {
                MString fileName;
                MGlobal::executeCommand( "file -q -ns;", fileName ); // get the Scene Name
                boost::regex fileRegex( "__[0-9]{4,5}$" );           // this Regex matches __####(EOS) and __#####(EOS)
                std::string file( fileName.asChar() );               // convert to std::string from MString
                std::string replaced =
                    boost::regex_replace( file, fileRegex, "" ); // replace the regex from within the file with ""
                MGlobal::executeCommand( "setAttr defaultRenderGlobals.imageFilePrefix -type \"string\" \"" +
                                         MString( replaced.c_str() ) + "\";" );
                // change the file name in the default render settings which krakatoa defaults to if given
            }
            std::vector<MDagPath> renderableCameras;
            maya_util::find_all_renderable_cameras( renderableCameras );

            if( renderableCameras.size() == 0 ) {
                FF_LOG( stats ) << "Krakatoa MY: No renderable cameras found, skipping render." << std::endl;
                return MStatus::kSuccess;
            }

            const size_t numFrames =
                (size_t)( ( renderData.frameEnd - renderData.frameStart ) / renderData.frameBy ).as( MTime::uiUnit() );
            const size_t numImages = numFrames + renderableCameras.size();

            size_t currentImage = 0;

            for( MTime timeIt = renderData.frameStart; timeIt <= renderData.frameEnd; timeIt += renderData.frameBy ) {
                krakatoasr::krakatoa_renderer krakRenderer;
                maya_ksr::mesh_context meshContext;

                maya_render_save_interface saveInterface( settingsNode );

                krakRenderer.set_render_save_callback( &saveInterface );

                // Although there are ways to query scene state at any arbitrary time in a more stateless fashion, such
                // methods do _not_ work for maya particle emitters, and also negate any possible 'compute'
                // optimizations that we implement in our nodes, which help improve render times.  This shouldn't be an
                // issue however, since batch renders are always executed in a seperate instance of maya anyways, and
                // thus wouldn't obstruct the user. It should be possible to render anything else using any arbitrary
                // scene time though if you are so inclined.

                setCurrentTime( timeIt );
                MDGContext currentContext( timeIt );

                maya_ksr::apply_scene_to_renderer( settingsNode, currentContext, krakRenderer, meshContext );

                for( size_t i = 0; i < renderableCameras.size(); ++i ) {

                    MFnCamera mayaCamera( renderableCameras[i] );

                    frantic::tstring cameraName = frantic::strings::to_tstring( mayaCamera.name().asChar() );

                    saveInterface.set_camera_name( cameraName );
                    saveInterface.set_current_context( currentContext );

                    maya_ksr::apply_camera_to_renderer( mayaCamera, settingsNode, currentContext, krakRenderer );

                    // launch the rendering function
                    launch_krakatoa_render( settingsNode, currentContext, krakRenderer );

                    MRenderUtil::sendRenderProgressInfo( saveInterface.get_output_image_name().c_str(),
                                                         int( double( currentImage ) / double( numImages ) * 100.0 ) );
                    ++currentImage;
                }
            }

            FF_LOG( stats ) << "Krakatoa MY: Finished Batch Render" << std::endl;
        } else {
            FF_LOG( stats ) << "Krakatoa MY: settings node was not found, cannot render." << std::endl;
        }

    } catch( std::runtime_error& e ) {
        std::ostringstream os;
        os << "Krakatoa MY: ERROR : " << e.what() << std::endl;
        FF_LOG( stats ) << os.str().c_str();
        MRenderUtil::sendRenderProgressInfo( os.str().c_str(), 0 );
        throw e; // in batch-render mode, we want to actually throw the exception so that deadline reports it as a
                 // failure.
    }

    return MStatus::kSuccess;
}

const MString RenderCommandListener::commandName = "RenderCommandListener";

void* RenderCommandListener::creator() { return new RenderCommandListener(); }

MStatus RenderCommandListener::doIt( const MArgList& args ) {
    MStatus status = MStatus::kSuccess;
    DumpArgs( args );

    if( MRenderView::doesRenderEditorExist() ) {
        std::cout << "Render editor exists" << std::endl;
    } else {
        std::cout << "Render editor _does_not_ exist" << std::endl;
    }

    return status;
}

const MString KrakatoaLocalBatchRenderOptionsString::commandName = "KrakatoaLocalBatchRenderOptionsString";

void* KrakatoaLocalBatchRenderOptionsString::creator() { return new KrakatoaLocalBatchRenderOptionsString; }

MStatus KrakatoaLocalBatchRenderOptionsString::doIt( const MArgList& args ) {
    MCommonRenderSettingsData renderSettings;
    MRenderUtil::getCommonRenderSettings( renderSettings );

    // be sure to escape all potentially offending shell characters
    std::string result = std::string( renderSettings.preMel.asChar() );
#if defined( WIN32 ) || defined( _WIN64 )
    boost::algorithm::replace_all( result, "\\", "\\\\" );
    boost::algorithm::replace_all( result, "\"", "\\\"" );
#endif
    boost::algorithm::replace_all( result, "\'", "\\\'" );

    if( result.length() > 0 ) {
        result += "; ";
    }

    result += "KrakatoaLocalBatchRender();";

#if defined( WIN32 ) || defined( _WIN64 )
    result = "\\\"" + result + "\\\"";
#else
    result = "\\\'" + result + "\\\'";
#endif

    result = "-preRender " + result;

    setResult( MString( result.c_str() ) );

    return MStatus::kSuccess;
}

const MString KrakatoaLocalBatchRender::commandName = "KrakatoaLocalBatchRender";
bool KrakatoaLocalBatchRender::s_flaggedAsLocalBatchRender = false;

void* KrakatoaLocalBatchRender::creator() { return new KrakatoaLocalBatchRender; }

bool KrakatoaLocalBatchRender::isLocalBatchRender() { return s_flaggedAsLocalBatchRender; }

MStatus KrakatoaLocalBatchRender::doIt( const MArgList& args ) {
    if( frantic::maya::is_batch_mode() )
        s_flaggedAsLocalBatchRender = true;
    return MStatus::kSuccess;
}
