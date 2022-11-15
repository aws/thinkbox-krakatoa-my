// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <krakatoasr_renderer.hpp>

#include <maya/MDGContext.h>
#include <maya/MFnDependencyNode.h>

#include <frantic/strings/tstring.hpp>

#include <krakatoasr_datatypes.hpp>

class maya_render_save_interface : public krakatoasr::render_save_interface {
  public:
    maya_render_save_interface( const MFnDependencyNode& krakatoaSettingsNode,
                                const MDGContext& currentContext = MDGContext::fsNormal,
                                const frantic::tstring& cameraName = _T("") );
    virtual ~maya_render_save_interface();

    virtual void save_render_data( int width, int height, int imageCount, const krakatoasr::output_type_t* listOfTypes,
                                   const krakatoasr::frame_buffer_pixel_data* const* listOfImages );

    void set_current_context( const MDGContext& currentContext );
    void set_camera_name( const frantic::tstring& cameraName );

    frantic::tstring get_output_image_name( const frantic::tstring& appendName = _T("") );
    frantic::tstring get_output_image_extension();

    krakatoasr::exr_compression_t get_exr_compression_type() const;
    krakatoasr::exr_bit_depth_t get_exr_bit_depth( const std::string& bitDepthName ) const;

  private:
    void save_velocity_image_data( int width, int height, const krakatoasr::frame_buffer_pixel_data* imageData );
    void save_rgba_image_data( int width, int height, const krakatoasr::frame_buffer_pixel_data* imageData );
    void save_depth_image_data( int width, int height, const krakatoasr::frame_buffer_pixel_data* imageData );
    void save_normal_image_data( int width, int height, const krakatoasr::frame_buffer_pixel_data* imageData );
    void save_occluded_image_data( int width, int height, const krakatoasr::frame_buffer_pixel_data* imageData );

    MDGContext m_currentContext;
    frantic::tstring m_cameraName;
    MFnDependencyNode m_krakatoaSettings;
};
