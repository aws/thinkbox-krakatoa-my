// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/strings/tstring.hpp>
#include <krakatoasr_renderer.hpp>
/**
 * Krakatoa SR frame buffer callback for maya
 */
class maya_frame_buffer_interface : public krakatoasr::frame_buffer_interface {
  private:
    frantic::strings::tstring m_cameraName;

  public:
    maya_frame_buffer_interface();
    ~maya_frame_buffer_interface();
    void set_camera_name( frantic::strings::tstring cameraName );

    virtual void set_frame_buffer( int width, int height, const krakatoasr::frame_buffer_pixel_data* data );
};
