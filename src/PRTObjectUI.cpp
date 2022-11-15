// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "PRTIconMesh.hpp"
#include "PRTObject.hpp"
#include "PRTObjectUI.hpp"

#include <frantic/maya/maya_util.hpp>
#include <frantic/maya/particles/particles.hpp>

#include <frantic/graphics/color3f.hpp>
#include <frantic/graphics/transform4f.hpp>
#include <frantic/graphics/vector3f.hpp>

#include <maya/MDagPath.h>
#include <maya/MDrawData.h>
#include <maya/MDrawRequestQueue.h>
#include <maya/MFnCamera.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MMatrix.h>
#include <maya/MSelectInfo.h>
#include <maya/MSelectionMask.h>

using namespace frantic;
using namespace frantic::graphics;
using namespace frantic::particles;
using namespace frantic::channels;
using namespace frantic::maya;

const float PRTObjectUI::s_smallPointSize = 1.0f;
const float PRTObjectUI::s_largePointSize = 4.0f;
const MString PRTObjectUI::drawClassification( "drawdb/geometry/prtobject" );

void* PRTObjectUI::creator() { return new PRTObjectUI; }

PRTObjectUI::PRTObjectUI() {}

PRTObjectUI::~PRTObjectUI() {}

/**
 * Called just before 'draw' by Maya to set up/cache required information for the render
 */
void PRTObjectUI::getDrawRequests( const MDrawInfo& drawInfo, bool objectAndActiveOnly,
                                   MDrawRequestQueue& requestQueue ) {
    MDrawData data;
    PRTObject* prtLoader = getPRTObject();
    getDrawData( prtLoader->getCachedViewportParticles(), data );
    MDrawRequest request = drawInfo.getPrototype( *this );
    request.setDrawData( data );
    requestQueue.add( request );
}

/**
 * Primary draw method called by Maya
 */
void PRTObjectUI::draw( const MDrawRequest& request, M3dView& view ) const {
    if( request.displayStyle() == M3dView::kBoundingBox ) {
        drawBoundingBox( request, view );
    } else {
        MDrawData data = request.drawData();
        particle_array& particles = *reinterpret_cast<particle_array*>( data.geometry() );
        view.beginGL();
        bool useColorChannel = false;

        // check whether the object is the currently selected object, and color it appropriately
        // otherwise, set the default color to black, and use the object's color channel
        if( request.displayStatus() == M3dView::kLead ) {
            glColor4f( 0.0f, 1.0f, 0.0f, 1.0f );
        } else {
            glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
            useColorChannel = true;
        }

        drawRootGeometry( view );
        drawParticles( particles, useColorChannel, view );
        view.endGL();
    }
}

/**
 * Uses OpenGL to draw the particles, based on the PRTLoader's currently selected viewport render geometry
 *
 * @param particles the set of particles to draw
 * @param useColorChannel set whether to draw using color
 * @param view the Maya M3dView instance
 */
void PRTObjectUI::drawParticles( const frantic::particles::particle_array& particles, bool useColorChannel,
                                 M3dView& view ) const {
    glPushAttrib( GL_CURRENT_BIT );
    PRTObject::display_mode_t displayMode = getPRTObject()->getViewportDisplayMode();

    if( displayMode == PRTObject::DISPLAYMODE_DOT1 || displayMode == PRTObject::DISPLAYMODE_DOT2 ) {
        drawPoints( particles, displayMode == PRTObject::DISPLAYMODE_DOT1 ? s_smallPointSize : s_largePointSize, view );
    } else {
        channel_cvt_accessor<vector3f> accessor;

        float scale = 1.0f;

        if( displayMode == PRTObject::DISPLAYMODE_NORMAL && particles.get_channel_map().has_channel( _T("Normal") ) ) {
            accessor = particles.get_channel_map().get_cvt_accessor<vector3f>(
                frantic::maya::particles::PRTNormalChannelName );
        } else if( displayMode == PRTObject::DISPLAYMODE_VELOCITY &&
                   particles.get_channel_map().has_channel( _T("Velocity") ) ) {
            accessor = particles.get_channel_map().get_cvt_accessor<vector3f>(
                frantic::maya::particles::PRTVelocityChannelName );
            scale = 1.0f / (float)maya_util::get_fps();
        } else if( displayMode == PRTObject::DISPLAYMODE_TANGENT &&
                   particles.get_channel_map().has_channel( _T("Tangent") ) ) {
            accessor = particles.get_channel_map().get_cvt_accessor<vector3f>(
                frantic::maya::particles::PRTTangentChannelName );
        }

        if( accessor.is_valid() )
            drawLines( particles, accessor, scale, view );
    }

    glPopAttrib();
}

/**
 * Uses OpenGL to draw a points at each particle's position
 *
 * @param particles the set of particles to draw
 * @param useColorChannel set whether to draw using color
 * @param pointSize the size of particles to draw
 */
void PRTObjectUI::drawPoints( const particle_array& particles, float pointSize, M3dView& view ) const {
    if( !particles.get_channel_map().has_channel( _T("Position") ) )
        return;

    channel_accessor<vector3f> posAccessor;
    channel_cvt_accessor<color3f> colorAccessor;

    bool hasColor = particles.get_channel_map().has_channel( _T("Color") );

    posAccessor = particles.get_channel_map().get_accessor<vector3f>( _T("Position") );
    if( hasColor )
        colorAccessor = particles.get_channel_map().get_cvt_accessor<color3f>( _T("Color") );

    enterMatrixTransform();
    // must hang onto the old size to avoid stomping on openGL state
    float lastPointSize;
    glGetFloatv( GL_POINT_SIZE, &lastPointSize );
    glPointSize( pointSize );

    // Draw Particles
    glBegin( GL_POINTS );
    glColor3f( 1.0f, 1.0f, 1.0f );
    for( particle_array::const_iterator it = particles.begin(); it != particles.end(); ++it ) {
        if( hasColor ) {
            color3f color = colorAccessor.get( *it );
            glColor3f( color.r, color.g, color.b );
        }
        vector3f position = posAccessor.get( *it );
        glVertex3f( position.x, position.y, position.z );
    }
    glEnd();

    // restore the old glPointSize
    glPointSize( lastPointSize );
    exitMatrixTransform();
}

/**
 * Uses OpenGL to draw a series of lines at each particle's position, using the accessor
 * provided to get an offset value (e.g. velocity)
 *
 * @param particles the set of particles to draw
 * @param deltaAccessor the channel map accessor to get the offset position for each draw line
 * @param useColorChannel set whether to draw using color
 */
void PRTObjectUI::drawLines( const particle_array& particles,
                             const frantic::channels::channel_cvt_accessor<vector3f>& deltaAccessor, float scale,
                             M3dView& view ) const {
    if( !particles.get_channel_map().has_channel( frantic::maya::particles::PRTPositionChannelName ) )
        return;

    channel_accessor<vector3f> posAccessor =
        particles.get_channel_map().get_accessor<vector3f>( frantic::maya::particles::PRTPositionChannelName );
    channel_cvt_accessor<color3f> colorAccessor;
    bool hasColor = particles.get_channel_map().has_channel( frantic::maya::particles::PRTColorChannelName );
    if( hasColor ) {
        colorAccessor =
            particles.get_channel_map().get_cvt_accessor<color3f>( frantic::maya::particles::PRTColorChannelName );
    }
    enterMatrixTransform();

    // Draw Lines
    glBegin( GL_LINES );
    glColor3f( 1.0f, 1.0f, 1.0f );
    for( particle_array::const_iterator it = particles.begin(); it != particles.end(); ++it ) {
        if( hasColor ) {
            color3f color = colorAccessor.get( *it );
            glColor3f( color.r, color.g, color.b );
        }

        vector3f position = posAccessor.get( *it );
        glVertex3f( position.x, position.y, position.z );
        const vector3f delta = deltaAccessor.get( *it );
        const vector3f lineEnd = position + ( delta * scale );
        glVertex3f( lineEnd.x, lineEnd.y, lineEnd.z );
    }
    glEnd();
    exitMatrixTransform();
}

/**
 * Draw a simple piece of geometry that is always present on the node, so as to
 * make it visible/pickable even if there are no particles.  Right now, just
 * draws a simple quad
 */
void PRTObjectUI::drawRootGeometry( M3dView& view ) const {

    gl_draw_trimesh( getPRTObject()->getRootMesh() );

    glBegin( GL_LINE_LOOP );
    glVertex3f( -0.5f, 0.0f, -0.5f );
    glVertex3f( -0.5f, 0.0f, 0.5f );
    glVertex3f( 0.5f, 0.0f, 0.5f );
    glVertex3f( 0.5f, 0.0f, -0.5f );
    glVertex3f( -0.5f, 0.0f, -0.5f );
    glEnd();

    /*
    glBegin( GL_QUADS );
    glVertex3f( -1.0f, 0.0f, -1.0f );
    glVertex3f( -1.0f, 0.0f,  1.0f );
    glVertex3f( 1.0f, 0.0f,  1.0f );
    glVertex3f( 1.0f, 0.0f, -1.0f );
    glEnd();
    */
}

/**
 * Draw a simple piece of geometry that is always present on the node, so as to
 * make it visible/pickable even if there are no particles.  Right now, just
 * draws a simple quad
 */
void PRTObjectUI::drawBoundingBox( const MDrawRequest& request, M3dView& view ) const {
    MDrawData data = request.drawData();
    particle_array& particles = *( reinterpret_cast<particle_array*>( data.geometry() ) );
    MBoundingBox bBox = getPRTObject()->boundingBox();
    float w = (float)bBox.width();
    float h = (float)bBox.height();
    float d = (float)bBox.depth();
    // This box drawing code is borrowed from the Maya API examples
    MPoint minVertex = bBox.min();
    view.beginGL();

    enterMatrixTransform();

    glPushAttrib( GL_CURRENT_BIT );
    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
    glBegin( GL_LINE_LOOP );
    MPoint vertex = minVertex;
    glVertex3f( (float)vertex[0], (float)vertex[1], (float)vertex[2] );
    glVertex3f( (float)vertex[0] + w, (float)vertex[1], (float)vertex[2] );
    glVertex3f( (float)vertex[0] + w, (float)vertex[1] + h, (float)vertex[2] );
    glVertex3f( (float)vertex[0], (float)vertex[1] + h, (float)vertex[2] );
    glVertex3f( (float)vertex[0], (float)vertex[1], (float)vertex[2] );
    glEnd();
    // Draw second side
    MPoint sideFactor( 0, 0, d );
    MPoint vertex2 = minVertex + sideFactor;
    glBegin( GL_LINE_LOOP );
    glVertex3f( (float)vertex2[0], (float)vertex2[1], (float)vertex2[2] );
    glVertex3f( (float)vertex2[0] + w, (float)vertex2[1], (float)vertex2[2] );
    glVertex3f( (float)vertex2[0] + w, (float)vertex2[1] + h, (float)vertex2[2] );
    glVertex3f( (float)vertex2[0], (float)vertex2[1] + h, (float)vertex2[2] );
    glVertex3f( (float)vertex2[0], (float)vertex2[1], (float)vertex2[2] );
    glEnd();
    // Connect the edges together
    glBegin( GL_LINES );
    glVertex3f( (float)vertex2[0], (float)vertex2[1], (float)vertex2[2] );
    glVertex3f( (float)vertex[0], (float)vertex[1], (float)vertex[2] );
    glVertex3f( (float)vertex2[0] + w, (float)vertex2[1], (float)vertex2[2] );
    glVertex3f( (float)vertex[0] + w, (float)vertex[1], (float)vertex[2] );
    glVertex3f( (float)vertex2[0] + w, (float)vertex2[1] + h, (float)vertex2[2] );
    glVertex3f( (float)vertex[0] + w, (float)vertex[1] + h, (float)vertex[2] );
    glVertex3f( (float)vertex2[0], (float)vertex2[1] + h, (float)vertex2[2] );
    glVertex3f( (float)vertex[0], (float)vertex[1] + h, (float)vertex[2] );
    glEnd();
    glPopAttrib();

    exitMatrixTransform();

    view.endGL();
}

/**
 * Check if this object has been picked.  Uses the OpenGL selection feature, so that
 * picking should match to exactly what is seen in the viewport
 */
bool PRTObjectUI::select( MSelectInfo& selectInfo, MSelectionList& selectionList,
                          MPointArray& worldSpaceSelectPts ) const {
    MFnSingleIndexedComponent fnComponent;
    MObject surfaceComponent = fnComponent.create( MFn::kMeshVertComponent );
    particle_array& particles = *( getPRTObject()->getCachedViewportParticles() );
    M3dView view = selectInfo.view();
    // this is a relatively inefficient way to perform selection (as opposed to manually doing the ray-casts ourselves),
    // but its accurate and it works.
    view.beginSelect();
    drawRootGeometry( view );
    drawParticles( particles, false, view );

    if( view.endSelect() > 0 ) {
        // This is the key part to making the selection right here
        // if you simply did this, the method would select the object whenever the selection ray intersected the
        // object's bounding box
        MSelectionMask priorityMask( MSelectionMask::kSelectObjectsMask );
        MSelectionList item;
        item.add( selectInfo.selectPath() );
        MPoint xformedPt;

        if( selectInfo.singleSelection() ) {
            MPoint center = getPRTObject()->boundingBox().center();
            xformedPt = center;
            xformedPt *= selectInfo.selectPath().inclusiveMatrix();
        }

        selectInfo.addSelection( item, xformedPt, selectionList, worldSpaceSelectPts, priorityMask, false );
        return true;
    } else {
        return false;
    }
}

PRTObject* PRTObjectUI::getPRTObject() const { return dynamic_cast<PRTObject*>( surfaceShape() ); }

static transform4f unMayaMatrix( 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 0.0f, 1.0f );

void PRTObjectUI::enterMatrixTransform() const {
    glPushAttrib( GL_TRANSFORM_BIT );
    glMatrixMode( GL_MODELVIEW );
    glPushMatrix();
    glPopAttrib();
}

void PRTObjectUI::exitMatrixTransform() const {
    glPushAttrib( GL_TRANSFORM_BIT );
    glMatrixMode( GL_MODELVIEW );
    glPopMatrix();
    glPopAttrib();
}
