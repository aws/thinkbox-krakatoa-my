// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <krakatoasr_renderer.hpp>
#include <maya/MFnCamera.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnLight.h>
#include <maya/MFnMesh.h>
#include <maya/MFnParticleSystem.h>

#include <krakatoasr_renderer/progress_logger.hpp>

#include <frantic/channels/channel_map.hpp>
#include <frantic/particles/streams/particle_istream.hpp>
#include <frantic/strings/tstring.hpp>

#include <boost/shared_array.hpp>
#include <boost/shared_ptr.hpp>

#include <map>

namespace maya_ksr {

// unfortunately, its neccessary to hang onto meshes that are created, as the renderer does not take explicit ownership
// of the meshes This class is just a primitive example of how to maintain this context It is designed such that in the
// future, it might be possible to associate meshes with a specific named attribute in the scene that outputs a mesh to
// more than one scene node, in order to support that kind of instancing
class mesh_context {
  private:
    typedef std::map<frantic::tstring, krakatoasr::triangle_mesh> mesh_container;
    mesh_container m_meshes;

  public:
    mesh_context();
    ~mesh_context();

    /**
     * First checks if a mesh was already registered with the specified name, otherwise returns a new instance
     */
    krakatoasr::triangle_mesh* get_shared_instance( const frantic::tstring& name );
};

struct bokeh_map_ptrs {
    boost::shared_array<krakatoasr::frame_buffer_pixel_data> shapeMapPtr;
    boost::shared_array<krakatoasr::frame_buffer_pixel_data> blendMapPtr;
};

// not including the camera
void apply_scene_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                              krakatoasr::krakatoa_renderer& krakRenderer, mesh_context& sharedMeshContext );

void apply_global_resolution_node_to_renderer( const MDGContext& currentContext,
                                               krakatoasr::krakatoa_renderer& krakRenderer );

void apply_shader_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                               krakatoasr::krakatoa_renderer& krakRenderer );

void apply_render_method_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                      krakatoasr::krakatoa_renderer& krakRenderer );

void apply_general_settings_to_renderer( const MFnDependencyNode& krakatoaSettingsNode,
                                         const MDGContext& currentContext,
                                         krakatoasr::krakatoa_renderer& krakRenderer );

void apply_current_camera_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                       krakatoasr::krakatoa_renderer& krakRenderer );

void apply_chosen_camera_to_renderer( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                      krakatoasr::krakatoa_renderer& krakRenderer, MString camera );

void apply_camera_to_renderer( const MFnCamera& mayaCamera, const MFnDependencyNode& krakatoaSettingsNode,
                               const MDGContext& currentContext, krakatoasr::krakatoa_renderer& krakRenderer );

void apply_light_to_renderer( const MFnLight& mayaLight, const MFnDependencyNode& krakatoaSettingsNode,
                              const MDGContext& currentContext, krakatoasr::krakatoa_renderer& krakRenderer );

void apply_load_percentage_to_renderer( const MFnDagNode& fnNode, const MFnDependencyNode& krakatoaSettingsNode,
                                        const MDGContext& currentContext, krakatoasr::krakatoa_renderer& krakRenderer );

void apply_prt_object_to_renderer( const MFnDagNode& fnPath, const MFnDependencyNode& prtNode,
                                   const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                   krakatoasr::krakatoa_renderer& krakRenderer );

void apply_maya_mesh_to_renderer( const MDagPath& fnNode, const MFnDependencyNode& krakatoaSettingsNode,
                                  const MDGContext& currentContext, krakatoasr::krakatoa_renderer& krakRenderer,
                                  mesh_context& sharedMeshContext );

void apply_common_operations_to_stream( const MFnDependencyNode& krakatoaSettingsNode, const MDGContext& currentContext,
                                        krakatoasr::particle_stream& particleStream );

bool has_krakatoa_matte_tag( const MFnDependencyNode& fnNode, const MDGContext& currentContext );

bool has_krakatoa_matte_shadows_tag( const MFnDependencyNode& fnNode, const MDGContext& currentContext );

void build_motion_transform( const MDagPath& dagNodePath, const MDGContext& currentContext, float shutterBeginOffset,
                             float shutterEndOffset, int numSamples, krakatoasr::animated_transform& outTForm );

void build_static_transform( const MDagPath& dagNodePath, const MDGContext& currentContext,
                             krakatoasr::animated_transform& outTForm );

void build_scene_transform( const MDagPath& dagNodePath, const MFnDependencyNode& krakatoaSettingsNode,
                            const MDGContext& currentContext, krakatoasr::animated_transform& outTForm );

int get_render_layer_setting( const std::string& settingName, const MDGContext& currentContext );

bokeh_map_ptrs apply_bokeh_settings_to_renderer_with_current_camera( const MFnDependencyNode& krakatoaSettingsNode,
                                                                     const MDGContext& currentContext,
                                                                     krakatoasr::krakatoa_renderer& krakRenderer );

bokeh_map_ptrs apply_bokeh_settings_to_renderer_with_chosen_camera( const MFnDependencyNode& krakatoaSettingsNode,
                                                                    const MDGContext& currentContext,
                                                                    krakatoasr::krakatoa_renderer& krakRenderer,
                                                                    MString camera );

/**
 * This is a utility function to set up a particle stream to match what the render would do.
 * This function is not called at render time. It is for those who need a stream to look exactly like it would look if
 * the render had proccessed it. This includes: Adding all the global color overrides, fractional streams, all object
 * modifiers, etc. It is used for: Viewport caching of streams, PRTExporter (partitioning), or anyone else that needs a
 * stream from a PRT object, and wants all the fanciness on it. It skips the global settings Krakatoa Settings node
 * doesn't exist, but it still processes modifiers.
 */
boost::shared_ptr<frantic::particles::streams::particle_istream>
get_renderer_stream_modifications( boost::shared_ptr<frantic::particles::streams::particle_istream> inStream,
                                   const MDGContext& currentContext );

} // namespace maya_ksr
