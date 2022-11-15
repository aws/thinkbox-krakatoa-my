// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once


#include <maya/M3dView.h>
#include <maya/MDrawInfo.h>
#include <maya/MDrawRequest.h>
#include <maya/MPxSurfaceShapeUI.h>
#include <maya/MSelectionList.h>

#include <frantic/channels/channel_accessor.hpp>
#include <frantic/particles/particle_array.hpp>

class PRTObject;

/**
 * User-Interface class to the PRTLoader, responsible for drawing and selection methods
 */
class PRTObjectUI : public MPxSurfaceShapeUI {
  public:
    static const MString drawClassification;

    static void* creator();

  public:
    PRTObjectUI();
    virtual ~PRTObjectUI();

    virtual void getDrawRequests( const MDrawInfo& drawInfo, bool objectAndActiveOnly, MDrawRequestQueue& requests );
    virtual void draw( const MDrawRequest& request, M3dView& view ) const;
    virtual bool select( MSelectInfo& selectInfo, MSelectionList& selectionList,
                         MPointArray& worldSpaceSelectPts ) const;

  private:
    static const float s_smallPointSize;
    static const float s_largePointSize;

  private:
    void drawBoundingBox( const MDrawRequest& request, M3dView& view ) const;

    void drawParticles( const frantic::particles::particle_array& particles, bool useColorChannel,
                        M3dView& view ) const;
    void drawPoints( const frantic::particles::particle_array& particles, float pointSize, M3dView& view ) const;
    void drawLines( const frantic::particles::particle_array& particles,
                    const frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f>& deltaAccessor,
                    float scale, M3dView& view ) const;
    void drawRootGeometry( M3dView& view ) const;

    void enterMatrixTransform() const;
    void exitMatrixTransform() const;

    PRTObject* getPRTObject() const;
};
