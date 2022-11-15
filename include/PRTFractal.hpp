// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "PRTObject.hpp"

#include <krakatoa/prt_stream_builder.hpp>

#include <boost/cstdint.hpp>
#include <boost/shared_ptr.hpp>
#include <frantic/channels/channel_map.hpp>
#include <frantic/geometry/trimesh3.hpp>
#include <frantic/graphics/color3f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/logging/logging_level.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/streams/particle_istream.hpp>
#include <krakatoasr_datatypes.hpp>
#include <maya/MDataBlock.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MPxSurfaceShape.h>
#include <maya/MPxSurfaceShapeUI.h>
#include <maya/MTime.h>
#include <maya/MVector.h>
// setkeys and RandomizeFractals
#include <maya/MArgList.h>
#include <maya/MPxCommand.h>
#include <maya/MString.h>

#if !( __APPLE__ && MAYA_API_VERSION >= 201800 )
class MIntArray;
class MVectorArray;
class MFnArrayAttrsData;
#endif

/**
 * Maya Plugin shape object that can be used to represent a set of PRT file sequences in the scene.
 */
class PRTFractal : public PRTObject {
  public:
    // Maya rtti and object creation information
    static void* creator();
    static MStatus initialize();
    static MTypeId typeId;
    static MString typeName;
    static const MString drawRegistrantId;
    static const MString drawClassification;

  public:
    PRTFractal();
    virtual ~PRTFractal();

    virtual void postConstructor();

    // inherited from MPxSurfaceShape
    virtual MBoundingBox boundingBox() const;
    virtual bool isBounded() const;

    // inherited from MPxNode
    virtual MStatus compute( const MPlug& plug, MDataBlock& block );
    virtual MStatus setDependentsDirty( const MPlug& plug, MPlugArray& plugArray );

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

    // test
    // static MObject inInputFiles;
    static MObject inAffineTransformationCount;

    // viewport particle loading mode
    static MObject inViewportLoadMode;

    // How to represent each particle (dot, velocity, etc..)
    static MObject inViewportDisplayMode;

    // a dummy output parameter that forces read-through of the input attributes
    static MObject outSentinel;

    static MObject inPosX[];
    static MObject inPosY[];
    static MObject inPosZ[];
    static MObject inRotX[];
    static MObject inRotY[];
    static MObject inRotZ[];
    static MObject inRotW[];
    static MObject inScaleX[];
    static MObject inScaleY[];
    static MObject inScaleZ[];
    static MObject inSkewX[];
    static MObject inSkewY[];
    static MObject inSkewZ[];
    static MObject inSkewW[];
    static MObject inSkewA[];
    static MObject inWeight[];
    static MObject inStartColor;
    static MObject inEndColor;

    static MObject inRenderParticleCount;
    static MObject inViewportParticleCount;

    static MObject inFractalRandomSeed;

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
    static void addDependencies( MObject obj );

    krakatoa::viewport::RENDER_LOAD_PARTICLE_MODE getViewportLoadMode() const;

    frantic::graphics::vector3f getVector3fAttribute( MObject attribute,
                                                      const MDGContext& context = MDGContext::fsNormal,
                                                      MStatus* outStatus = NULL ) const;
    int getIntAttribute( MObject attribute, const MDGContext& context = MDGContext::fsNormal,
                         MStatus* outStatus = NULL ) const;
    double getDoubleAttribute( MObject attribute, const MDGContext& context = MDGContext::fsNormal,
                               MStatus* outStatus = NULL ) const;
    bool getBooleanAttribute( MObject attribute, const MDGContext& context = MDGContext::fsNormal,
                              MStatus* outStatus = NULL ) const;
    MTime getTimeAttribute( MObject attribute, const MDGContext& context = MDGContext::fsNormal,
                            MStatus* outStatus = NULL ) const;

    // return channel map for displaying viewport based on the channel map of inputStream
    // "Position", "Color" will be returned if inputStream have these channels
    frantic::channels::channel_map
    get_viewport_channel_map( boost::shared_ptr<frantic::particles::streams::particle_istream> inputStream ) const;

  private:
    frantic::particles::particle_array m_cachedParticles;
    MBoundingBox m_boundingBox;
    bool m_osxViewport20HackInitialized;
};

class AddFractalTransformKeyframes : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();

    static void setPluginPath( const MString& path );
    static const MString getPluginPath();

  private:
    static MString s_pluginPath;

  public:
    MStatus doIt( const MArgList& args );
};

class RandomizeFractals : public MPxCommand {
  public:
    static const MString commandName;
    static void* creator();

    static void setPluginPath( const MString& path );
    static const MString getPluginPath();

  private:
    static MString s_pluginPath;

  public:
    MStatus doIt( const MArgList& args );
};
