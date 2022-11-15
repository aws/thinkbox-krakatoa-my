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
#include <maya/MTime.h>
#include <maya/MVector.h>

#include <krakatoa/particle_volume.hpp>

#include <frantic/channels/channel_map.hpp>
#include <frantic/graphics/color3f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/logging/logging_level.hpp>
#include <frantic/logging/progress_logger.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/particle_istream.hpp>
#include <frantic/volumetrics/levelset/rle_level_set.hpp>

#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>

#if !( __APPLE__ && MAYA_API_VERSION >= 201800 )
class MIntArray;
class MVectorArray;
class MFnArrayAttrsData;
#endif

/**
 * Maya Plugin shape object that can be used to represent a cloud of particles generated from the internal volume of a
 * mesh
 */
class PRTVolume : public PRTObject {
  public:
    typedef boost::shared_ptr<frantic::volumetrics::levelset::rle_level_set> level_set_ptr;
    typedef boost::shared_ptr<krakatoa::particle_volume_voxel_sampler> level_set_sampler_ptr;

  public:
    // Maya rtti and object creation information
    static void* creator();
    static MStatus initialize();
    static MTypeId typeId;
    static MString typeName;
    static const MString drawRegistrantId;

  public:
    PRTVolume();
    virtual ~PRTVolume();

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
    // The mesh from which the particle volume is calculated
    static MObject inMesh;

    // The 'worldMatrix' of the mesh used by the source object
    static MObject inMeshTransform;

    // The 'worldMatrix' of this object (For some reason, MFnAttribute::setWorldSpace does not work so this is used as a
    // work around)
    static MObject inCurrentTransform;

    // boolean that states if the volume should be rendered in object or world-space
    static MObject inUseWorldSpace;

    // The sampling spacing between particles to use in the render
    static MObject inRenderSpacing;

    // Specify whether to use the custom spacing in the viewport, or just use the default render spacing
    static MObject inUseCustomViewportSpacing;

    // the spacing spacing between particles to use in the viewport
    // typically this is set to a higher value in order to retain the shape of the object while reducing viewport render
    // load
    static MObject inViewportSpacing;

    // Define whether to use multiple subdivisions
    static MObject inSubdivideVoxel;

    // Number of subdivisions to use if enabled
    static MObject inNumSubdivisions;

    // Specify whether the particles should be affected by random jitter, or distributed uniformly
    static MObject inRandomJitter;

    // If random jitter is enabled, this helps ensure that the volume is well-filled (lessens possible gaps and such)
    static MObject inJitterWellDistributed;

    // I have no idea what this is supposed to do
    static MObject inJitterMultiplePerVoxel;

    // I have no idea what this is supposed to do
    static MObject inJitterCountPerVoxel;

    // This integer defines the seed for the random jitter.
    static MObject inRandomSeed;

    // I gather this is how many times the random generator is called with the given seed when generating the volume
    static MObject inNumDistinctRandomValues;

    // use density compensation
    static MObject inUseDensityCompensation;

    // whether to show the volume particles in the viewport at all
    static MObject inEnableInViewport;

    // Disable subdivisions in the viewport
    static MObject inViewportDisableSubdivision;

    // Percentage of the total number of particles to display in the viewport
    static MObject inViewportParticlePercent;

    // Enable/disable the control over viewport load limit
    static MObject inEnableViewportParticleLimit;

    // Maximum number of particles to display in the viewport
    static MObject inViewportParticleLimit;

    // Whether or not to use a 'surface shell', rather than just fill the entire volume
    static MObject inUseSurfaceShell;

    // The distance from the object surface at which to start emitting particles
    static MObject inSurfaceShellStart;

    // The thickness of the surface
    static MObject inSurfaceShellThickness;

    // The rendering method to use when displaying the particles in the viewport
    static MObject inViewportDisplayMode;

    // This acts as a proxy to determine if the level-set needs to be recomputed, based on whether
    // the input mesh attribute has changed
    static MObject outViewportMeshLevelSetProxy;

    // This acts as a proxy to determine if the level-set needs to be recomputed
    static MObject outViewportLevelSetSamplerProxy;

    // This acts as a proxy to determine if the subdivisions for the level set sampler need to be recomputed
    static MObject outViewportLevelSetSamplerSubdivisionsProxy;

    // a dummy output parameter that forces read-through of the input attributes
    // This can be thought of as the proxy for the cached particle array on this object
    static MObject outViewportParticleCacheProxy;

    static MObject outRenderMeshLevelSetProxy;
    static MObject outRenderLevelSetSamplerProxy;
    static MObject outRenderLevelSetSamplerSubdivisionsProxy;

    // output particle stream
    static MObject outParticleStream;

    static boost::unordered_set<std::string> viewportInputDependencies;

  private:
    void touchSentinelOutput( MObject sentinelAttribute ) const;
    void cacheViewportParticles();
    PRTObject::particle_istream_ptr buildParticleStream( level_set_ptr levelSet, level_set_sampler_ptr sampler ) const;
    level_set_ptr buildLevelSet( MDataBlock& inputDataBlock, bool viewportMode ) const;
    level_set_sampler_ptr buildLevelSetSampler( bool viewportMode ) const;
    void cacheBoundingBox();

    double getViewportSpacing() const;
    double getRenderSpacing() const;

    double getViewportParticleFraction() const;
    boost::int64_t getViewportParticleLimit() const;

    bool inWorldSpace() const;
    frantic::graphics::transform4f getWorldSpaceTransform( const MDGContext& context, bool* isOK = NULL ) const;
    int getViewportSubdivisionCount() const;
    int getRenderSubdivisionCount() const;
    bool isJitterEnabled() const;
    int getViewportJitterPerVoxelCount() const;
    int getRenderJitterPerVoxelCount() const;
    bool isMultipleJitterEnabled() const;
    int getRandomSeed() const;
    int getRandomCount() const;
    bool isJitterWellDistributed() const;
    bool useDensityCompensation() const;

    double getShellStart() const;
    double getShellThickness() const;

    level_set_ptr getCachedViewportLevelSet();
    level_set_sampler_ptr getCachedViewportLevelSetSampler();

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
    level_set_ptr m_cachedMeshLevelSet;
    level_set_sampler_ptr m_cachedLevelSetSampler;
    level_set_ptr m_cachedRenderMeshLevelSet;
    level_set_sampler_ptr m_cachedRenderLevelSetSampler;
    bool m_osxViewport20HackInitialized;
};
