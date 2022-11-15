// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "PRTObject.hpp"

#include <maya/MDataBlock.h>
#include <maya/MFnMesh.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MPxSurfaceShape.h>
#include <maya/MPxSurfaceShapeUI.h>
#include <maya/MVector.h>

#include <frantic/channels/channel_map.hpp>
#include <frantic/graphics/color3f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/particles/streams/particle_istream.hpp>

#include <boost/shared_ptr.hpp>

/**
 * Maya Plugin shape object that can be used to represent a cloud of particles generated from the surface of a mesh
 * Base code was copied and modified from PRTVolume
 */
class PRTSurface : public PRTObject {

  public:
    // Maya rtti and object creation information
    static void* creator();
    static MStatus initialize();
    static MTypeId typeId;
    static MString typeName;
    static const MString drawRegistrantId;

  public:
    PRTSurface();
    virtual ~PRTSurface();

    virtual void postConstructor();

    // inherited from MPxSurfaceShape
    virtual MBoundingBox boundingBox() const;
    virtual bool isBounded() const;

    // inherited from MPxNode
    virtual MStatus compute( const MPlug& plug, MDataBlock& block );
    virtual MStatus setDependentsDirty( const MPlug& plug, MPlugArray& plugArray );

    // inherited from PRTObject
    virtual PRTObject::particle_istream_ptr getRenderParticleStream( const frantic::graphics::transform4f& tm,
                                                                     const MDGContext& currentContext ) const;
    virtual PRTObject::particle_istream_ptr getViewportParticleStream( const frantic::graphics::transform4f& tm,
                                                                       const MDGContext& currentContext ) const;

    virtual frantic::particles::particle_array* getCachedViewportParticles();
    virtual PRTObject::display_mode_t getViewportDisplayMode();

    virtual const frantic::geometry::trimesh3& getRootMesh() const;

  private:
    // The mesh from which the particle surface is calculated
    static MObject inMesh;

    // The 'worldMatrix' of the mesh used by the source object
    static MObject inMeshTransform;

    // The 'worldMatrix' of this object (For some reason, MFnAttribute::setWorldSpace does not work so this is used as a
    // work around)
    static MObject inCurrentTransform;

    // boolean that states if the surface should be rendered in object or world-space
    static MObject inUseWorldSpace;

    // This integer defines the seed for the random jitter.
    static MObject inRandomSeed;

    // This integer defines the total number of particles to generate.
    static MObject inParticleCount;

    // Enable/Disable override to use spacing.
    static MObject inUseParticleSpacing;

    // This double defines the spacing of the particles to generate.
    static MObject inParticleSpacing;

    // use density compensation
    static MObject inUseDensityCompensation;

    // whether to show the surface particles in the viewport at all
    static MObject inEnableInViewport;

    // Percentage of the total number of particles to display in the viewport
    static MObject inViewportParticlePercent;

    // Enable/disable the control over viewport load limit
    static MObject inEnableViewportParticleLimit;

    // Maximum number of particles to display in the viewport
    static MObject inViewportParticleLimit;

    // The rendering method to use when displaying the particles in the viewport
    static MObject inViewportDisplayMode;

    // a dummy output parameter that forces read-through of the input attributes
    // This can be thought of as the proxy for the cached particle array on this object
    static MObject outViewportParticleCacheProxy;

    // output particle stream
    static MObject outParticleStream;

    static boost::unordered_set<std::string> viewportInputDependencies;

  private:
    void touchSentinelOutput( MObject sentinelAttribute ) const;
    void cacheViewportParticles();
    PRTObject::particle_istream_ptr buildParticleStream( bool viewportMode ) const;
    void cacheBoundingBox();

    double getViewportSpacing() const;
    double getRenderSpacing() const;

    double getViewportParticleFraction() const;
    boost::int64_t getViewportParticleLimit() const;

    bool inWorldSpace() const;
    frantic::graphics::transform4f getWorldSpaceTransform( const MDGContext& context, bool* isOK = NULL ) const;
    MMatrix getMayaWorldSpaceTransform( const MDGContext& context, bool* isOK = NULL ) const;
    int getRandomSeed() const;
    int getParticleCount() const;
    double getParticleSpacing() const;
    bool isUseParticleSpacing() const;
    bool useDensityCompensation() const;

    int getIntAttribute( MObject attribute, MDGContext& context = MDGContext::fsNormal,
                         MStatus* outStatus = NULL ) const;
    double getDoubleAttribute( MObject attribute, MDGContext& context = MDGContext::fsNormal,
                               MStatus* outStatus = NULL ) const;
    bool getBooleanAttribute( MObject attribute, MDGContext& context = MDGContext::fsNormal,
                              MStatus* outStatus = NULL ) const;

    // return channel map for displaying viewport based on the channel map of inputStream
    // "Position", "Velocity", "Normal", "Color" will be returned if inputStream have these channels
    frantic::channels::channel_map
    get_viewport_channel_map( boost::shared_ptr<frantic::particles::streams::particle_istream> inputStream ) const;

  private:
    frantic::particles::particle_array m_cachedParticles;
    MBoundingBox m_boundingBox;
    bool lastWorldSpaceState;
    bool m_osxViewport20HackInitialized;
};
