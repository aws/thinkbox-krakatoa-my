// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <boost/unordered_set.hpp>

#include <maya/MPxSurfaceShape.h>

#include <frantic/maya/MPxParticleStream.hpp>
#include <frantic/maya/PRTObject_base.hpp>
#include <krakatoasr_transformation.hpp>

class PRTObject : public MPxSurfaceShape, public frantic::maya::PRTObjectBase {
  public:
    enum display_mode_t {
        DISPLAYMODE_DOT1 = 1,
        DISPLAYMODE_DOT2,
        DISPLAYMODE_VELOCITY,
        DISPLAYMODE_NORMAL,
        DISPLAYMODE_TANGENT
    };

  public:
    virtual ~PRTObject();

    // inherited from MPxNode
    virtual void postConstructor();

    /**
     * Gets a transform for this PRTObject.
     * This is used for viewport caching purposes only.  Use the one that takes in a MDagPath to get the proper
     * transform
     */
    virtual frantic::graphics::transform4f getTransform( const MDGContext& context ) const;

    /**
     * Returns a cached particle_array of the particle set suitable for displaying in the viewport.
     * This will only be re-computed if the current scene context changes.
     */
    virtual frantic::particles::particle_array* getCachedViewportParticles() = 0;

    /**
     * Specifies the display mode to use when drawing the particles in the viewport.  Ensure that the appropriately
     * named channel exists corresponding to the returned display mode.
     */
    virtual display_mode_t getViewportDisplayMode() = 0;

    /**
     * Returns the root display mesh to show under this object in the scene
     * @return a const reference to the root display trimesh
     */
    virtual const frantic::geometry::trimesh3& getRootMesh() const;

#if MAYA_API_VERSION >= 201600
    /**
     * Returns a selection mask for Viewport 2.0 Selection
     */
    virtual MSelectionMask getShapeSelectionMask() const;
#endif

    /**
     * For Viewport 2.0, this registers an attribute that will require the viewport to redraw. For Viewport 1.0, this
     * will set the attribute so that it will affect the affected object(ie an update to attribute will cause and update
     * to affected).
     */
    static void register_viewport_dependency( boost::unordered_set<std::string>& registry, MObject& attr,
                                              MObject& affected );

    /**
     * Viewport 2.0 code, which checks if an element requires the viewport to be redrawn.
     */
    static void check_dependents_dirty( boost::unordered_set<std::string>& registry, const MPlug& attr,
                                        MObject& toUpdates );

    /**
     * TOTAL HACK FOR BECAUSE FOR VIEWPORT 2.0 UPDATING ON OS X
     * On OS X, the function "setDependentsDirty" never seems to be called, therefore, the neccessary
     * "MHWRender::MRenderer::setGeometryDrawDirty" function is not called to alert viewport 2.0 that it needs to
     * update. So instead, we're registering a scriptJob each time to run this Mel script command to force it to update.
     * We should think of removing this if it is fixed, or the problem source is found. Check again if it's still a
     * problem in Maya 2016+.
     */
    static void
    register_osx_viewport20_update_hack( MString objName,
                                         const boost::unordered_set<std::string>& viewportInputDependencies );
};
