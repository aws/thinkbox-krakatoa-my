// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "maya_frame_buffer_interface.hpp"

#include <vector>

#include <frantic/logging/logging_level.hpp>

#include <maya/M3dView.h>
#include <maya/MGlobal.h>
#include <maya/MRenderView.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>

using namespace krakatoasr;

maya_frame_buffer_interface::maya_frame_buffer_interface() { m_cameraName = _T(""); }

maya_frame_buffer_interface::~maya_frame_buffer_interface() {}
void maya_frame_buffer_interface::set_camera_name( frantic::strings::tstring cameraName ) { m_cameraName = cameraName; }

void maya_frame_buffer_interface::set_frame_buffer( int width, int height, const frame_buffer_pixel_data* data ) {
    MStatus status;
    bool output = false;
    if( MRenderView::doesRenderEditorExist() ) {
        if( m_cameraName.empty() ) {
            M3dView curView = M3dView::active3dView();
            MDagPath camDagPath;
            curView.getCamera( camDagPath );
            MRenderView::setCurrentCamera( camDagPath );
        } else {
            MSelectionList list;
            MGlobal::getSelectionListByName( MString( m_cameraName.c_str() ), list );
            MDagPath camDagPath;
            list.getDagPath( 0, camDagPath );
            MRenderView::setCurrentCamera( camDagPath );
        }

        // copy our entire buffer into a Maya buffer
        // we can do it pixel-by-pixel calls to "updatePixels" to save memory too, but updating as one big block seems
        // to be a tiny bit faster.
        std::vector<RV_PIXEL> pixelBlock( width * height );
        for( int i = 0; i < width * height; ++i ) {
            const frame_buffer_pixel_data& srcPixel = data[i];
            RV_PIXEL& destPixel = pixelBlock[i];
            destPixel.r = srcPixel.r;
            destPixel.g = srcPixel.g;
            destPixel.b = srcPixel.b;
            destPixel.a = ( ( srcPixel.r_alpha + srcPixel.g_alpha + srcPixel.b_alpha ) / 3.0f );
        }

        // give the frame buffer memory to maya.
        MRenderView::startRender( width, height, true, false );
        MRenderView::updatePixels( 0, width - 1, 0, height - 1, &pixelBlock[0], true );
        MRenderView::endRender();

    } else {
        FF_LOG( stats ) << "Krakatoa MY: Unable to create maya render viewer." << std::endl;
    }
}
