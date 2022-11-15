// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "PRTObject.hpp"

#include <krakatoa/prt_stream_builder.hpp>

#include <maya/MDataBlock.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MPxSurfaceShapeUI.h>
#include <maya/MTime.h>
#include <maya/MVector.h>

#include <frantic/channels/channel_map.hpp>
#include <frantic/geometry/trimesh3.hpp>
#include <frantic/graphics/color3f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/logging/logging_level.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/particle_istream.hpp>

#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>

#if !( __APPLE__ && MAYA_API_VERSION >= 201800 )
class MIntArray;
class MVectorArray;
class MFnArrayAttrsData;
#endif

struct input_file_info {
    frantic::tstring baseName;
    bool singleFileOnly;
    bool inViewport;
    bool inRender;
};

/**
 * Maya Plugin shape object that can be used to represent a set of PRT file sequences in the scene.
 */
class PRTLoader : public PRTObject {
  public:
    // Maya rtti and object creation information
    static void* creator();
    static MStatus initialize();
    static MTypeId typeId;
    static MString typeName;
    static const MString drawRegistrantId;

  public:
    PRTLoader();
    virtual ~PRTLoader();

    virtual void postConstructor();

    // inherited from MPxSurfaceShape
    virtual MBoundingBox boundingBox() const;
    virtual bool isBounded() const;

    // inherited from MPxNode
    virtual MStatus compute( const MPlug& plug, MDataBlock& block );
    virtual MStringArray getFilesToArchive( bool shortName = false, bool unresolvedName = false,
                                            bool markCouldBeImageSequence = false ) const;
    virtual MStatus setDependentsDirty( const MPlug& plug, MPlugArray& plugArray );

    bool isSingleSimpleRenderFile( MTime currentTime, frantic::tstring& outFileName ) const;

    // inherited from PRTObject
    virtual PRTObject::particle_istream_ptr
    getRenderParticleStream( const frantic::graphics::transform4f& tm,
                             const MDGContext& currentContext = MDGContext::fsNormal ) const;
    virtual PRTObject::particle_istream_ptr
    getViewportParticleStream( const frantic::graphics::transform4f& tm,
                               const MDGContext& currentContext = MDGContext::fsNormal ) const;

    virtual frantic::particles::particle_array* getCachedViewportParticles();
    virtual PRTObject::display_mode_t getViewportDisplayMode();

    virtual const frantic::geometry::trimesh3& getRootMesh() const;

  private:
    // current time, as received from the global time node
    static MObject inTime;

    // the set of PRT files to be attached to this node
    static MObject inPRTFile;

    // boolean specifying if a given file is part of the viewport view or not
    static MObject inUseFileInViewport;

    // boolean specifying if a given file is part of the render or not
    static MObject inUseFileInRender;

    // boolean specifying if a given file is meant to be just a single loaded file, or part of a file sequence
    static MObject inSingleFileOnly;

    // test
    static MObject inInputFiles;

    // if 'single frame only' is selected, then by default the velocity channel will not be preserved
    static MObject inKeepVelocityChannel;

    // select if more complex sub-frame interpolation should be performed
    static MObject inInterpolateSubFrames;

    // playback time re-mapping graph
    static MObject inPlaybackGraph;

    // use custom playback graph
    static MObject inEnablePlaybackGraph;

    // set a global frame offset for loading from the particle sequences
    static MObject inFrameOffset;

    // set whether to limit playback to a specific range of frames
    static MObject inUseCustomRange;

    // set the low end of the custom range
    static MObject inCustomRangeStart;

    // set the high end of the custom range
    static MObject inCustomRangeEnd;

    // set how to handle frames before the start of the custom range
    static MObject inCustomRangeStartClampMode;

    // set how to handle frames after the end of the custom range
    static MObject inCustomRangeEndClampMode;

    // viewport particle loading mode
    static MObject inViewportLoadMode;

    // viewport particle hard limit (this value, x1000)
    static MObject inViewportParticleLimit;

    // enable particle hard limit in viewport
    static MObject inEnableViewportParticleLimit;

    // percent of total number of particles to load in viewport
    static MObject inViewportParticlePercent;

    // How to represent each particle (dot, velocity, etc..)
    static MObject inViewportDisplayMode;

    // render particle loading mode
    static MObject inRenderLoadMode;

    // render particle hard limit (this value, x1000)
    static MObject inRenderParticleLimit;

    // enable particle hard limit in render
    static MObject inEnableRenderParticleLimit;

    // percent of total number of particles to load in render
    static MObject inRenderParticlePercent;

    // a dummy output parameter that forces read-through of the input attributes
    static MObject outSentinel;

    // the total number of particles in all referenced files
    static MObject outNumParticlesOnDisk;

    // the total number of particles that will be included in the render
    static MObject outNumParticlesInRender;

    // the total number of particles that will be displayed in the viewport
    static MObject outNumParticlesInViewport;

    // a hidden output parameter that can be used when optimizing render output
    // A non-empty string value indicates that the entire output render can be
    // represented by that filename.
    static MObject outSingleSimpleRenderFile;

    // output particle stream
    static MObject outParticleStream;

    static boost::unordered_set<std::string> viewportInputDependencies;

  private:
    particle_istream_ptr getParticleStream( const MDGContext& currentContext,
                                            krakatoa::enabled_mode::enabled_mode_enum mode ) const;

    void touchSentinelOutput() const;
    void cacheParticlesAt( MTime time );
    void cacheBoundingBox();

    MStatus getCurrentTime( MDataBlock& block, MTime& outTime ) const;

    MStatus getInputFiles( std::vector<input_file_info>& outFiles ) const;

    int getFrameOffset() const;
    bool interpolateSubFrames() const;
    bool useCustomRange() const;
    bool keepVelocityChannel() const;

    int getRangeStart() const;
    int getRangeEnd() const;
    krakatoa::clamp_mode::clamp_mode_enum getRangeStartClampMode() const;
    krakatoa::clamp_mode::clamp_mode_enum getRangeEndClampMode() const;

    MTime getEffectivePlaybackTime( MTime atTime ) const;

    krakatoa::viewport::RENDER_LOAD_PARTICLE_MODE getViewportLoadMode() const;
    double getViewportParticleFraction( const MDGContext& context = MDGContext::fsNormal ) const;
    boost::int64_t getViewportParticleLimit() const;

    // This calls take into account possibly using the render mode for the viewport mode
    krakatoa::render::RENDER_LOAD_PARTICLE_MODE getEffectiveViewportLoadMode() const;

    krakatoa::render::RENDER_LOAD_PARTICLE_MODE getRenderLoadMode() const;
    double getRenderParticleFraction( const MDGContext& context = MDGContext::fsNormal ) const;
    boost::int64_t getRenderParticleLimit() const;

    int getIntAttribute( MObject attribute, const MDGContext& context = MDGContext::fsNormal,
                         MStatus* outStatus = NULL ) const;
    double getDoubleAttribute( MObject attribute, const MDGContext& context = MDGContext::fsNormal,
                               MStatus* outStatus = NULL ) const;
    bool getBooleanAttribute( MObject attribute, const MDGContext& context = MDGContext::fsNormal,
                              MStatus* outStatus = NULL ) const;
    MTime getTimeAttribute( MObject attribute, const MDGContext& context = MDGContext::fsNormal,
                            MStatus* outStatus = NULL ) const;

    // return channel map for displaying viewport based on the channel map of inputStream
    // "Position", "Velocity", "Normal", "Color", "Tangent" will be returned if inputStream have these channels
    frantic::channels::channel_map
    get_viewport_channel_map( boost::shared_ptr<frantic::particles::streams::particle_istream> inputStream ) const;

  private:
    frantic::particles::particle_array m_cachedParticles;
    MBoundingBox m_boundingBox;
    bool m_osxViewport20HackInitialized;
};
