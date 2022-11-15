// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "PRTVolume.hpp"
#include "PRTVolumeIconMesh.hpp"

#include "maya_ksr.hpp"
#include "maya_progress_bar_interface.hpp"
#include <frantic/maya/maya_util.hpp>
#include <frantic/maya/particles/particles.hpp>

#include <krakatoasr_renderer/progress_logger.hpp>

#include <maya/MAnimControl.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MGlobal.h>

#include <frantic/maya/convert.hpp>
#include <frantic/maya/geometry/mesh.hpp>
#include <frantic/maya/util.hpp>

#include <frantic/geometry/trimesh3.hpp>
#include <frantic/graphics/transform4f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/particles/streams/empty_particle_istream.hpp>
#include <frantic/particles/streams/fractional_particle_istream.hpp>
#include <frantic/particles/streams/rle_levelset_particle_istream.hpp>
#include <frantic/particles/streams/transformed_particle_istream.hpp>

#include <boost/cstdint.hpp>
#include <limits>

using namespace frantic::particles;
using namespace frantic::particles::streams;
using namespace frantic::graphics;
using namespace frantic::geometry;
using namespace frantic::maya;

const MString PRTVolume::drawRegistrantId( "PrtVolumePlugin" );

// general parameters
MObject PRTVolume::inMesh;
MObject PRTVolume::inMeshTransform;
MObject PRTVolume::inCurrentTransform;
MObject PRTVolume::inUseWorldSpace;
MObject PRTVolume::inUseDensityCompensation;
MObject PRTVolume::outViewportParticleCacheProxy;
MObject PRTVolume::outViewportMeshLevelSetProxy;
MObject PRTVolume::outViewportLevelSetSamplerProxy;
MObject PRTVolume::outViewportLevelSetSamplerSubdivisionsProxy;
MObject PRTVolume::outRenderMeshLevelSetProxy;
MObject PRTVolume::outRenderLevelSetSamplerProxy;
MObject PRTVolume::outRenderLevelSetSamplerSubdivisionsProxy;

// spacing parameters
MObject PRTVolume::inRenderSpacing;
MObject PRTVolume::inUseCustomViewportSpacing;
MObject PRTVolume::inViewportSpacing;

// subdivision parameters
MObject PRTVolume::inSubdivideVoxel;
MObject PRTVolume::inNumSubdivisions;

// jitter parameters
MObject PRTVolume::inRandomJitter;
MObject PRTVolume::inJitterWellDistributed;
MObject PRTVolume::inJitterMultiplePerVoxel;
MObject PRTVolume::inJitterCountPerVoxel;
MObject PRTVolume::inRandomSeed;
MObject PRTVolume::inNumDistinctRandomValues;

// viewport load parameters
MObject PRTVolume::inViewportDisplayMode;
MObject PRTVolume::inEnableInViewport;
MObject PRTVolume::inViewportDisableSubdivision;
MObject PRTVolume::inViewportParticlePercent;
MObject PRTVolume::inEnableViewportParticleLimit;
MObject PRTVolume::inViewportParticleLimit;

// surface shell parameters
MObject PRTVolume::inUseSurfaceShell;
MObject PRTVolume::inSurfaceShellStart;
MObject PRTVolume::inSurfaceShellThickness;

// Output Particles
MObject PRTVolume::outParticleStream;

boost::unordered_set<std::string> PRTVolume::viewportInputDependencies;

MStatus PRTVolume::initialize() {
    MStatus status;
    // MFnUnitAttribute fnUnitAttribute;
    // MFnCompoundAttribute fnCompoundAttribute;
    // MFnStringData fnStringData;

    // Particle Stream Output
    {
        MFnTypedAttribute fnTypedAttribute;
        outParticleStream = fnTypedAttribute.create( "outParticleStream", "outParticleStream",
                                                     frantic::maya::MPxParticleStream::id, MObject::kNullObj );
        fnTypedAttribute.setHidden( true );
        fnTypedAttribute.setStorable( false );
        status = addAttribute( outParticleStream );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    // inMesh
    {
        MFnTypedAttribute fnTypedAttribute;
        inMesh = fnTypedAttribute.create( "inMesh", "mesh", MFnData::kMesh );
        status = addAttribute( inMesh );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inMeshTransform
    {
        MFnTypedAttribute fnTypedAttribute;
        inMeshTransform = fnTypedAttribute.create( "inMeshTransform", "meshXForm", MFnData::kMatrix );
        fnTypedAttribute.setHidden( true );
        status = addAttribute( inMeshTransform );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inUseWorldSpace
    {
        MFnNumericAttribute fnNumericAttribute;
        inUseWorldSpace = fnNumericAttribute.create( "inUseWorldSpace", "worldSpace", MFnNumericData::kBoolean, 0 );
        status = addAttribute( inUseWorldSpace );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inRenderSpacing
    {
        MFnNumericAttribute fnNumericAttribute;
        inRenderSpacing = fnNumericAttribute.create( "inRenderSpacing", "spacing", MFnNumericData::kFloat, 0.25 );
        status = addAttribute( inRenderSpacing );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inUseCustomViewportSpacing
    {
        MFnNumericAttribute fnNumericAttribute;
        inUseCustomViewportSpacing = fnNumericAttribute.create( "inUseCustomViewportSpacing", "useViewportSpacing",
                                                                MFnNumericData::kBoolean, 1 );
        status = addAttribute( inUseCustomViewportSpacing );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inViewportSpacing
    {
        MFnNumericAttribute fnNumericAttribute;
        inViewportSpacing =
            fnNumericAttribute.create( "inViewportSpacing", "viewportSpacing", MFnNumericData::kFloat, 0.5 );
        status = addAttribute( inViewportSpacing );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        inUseDensityCompensation = fnNumericAttribute.create( "inUseDensityCompensation", "densityCompensation",
                                                              MFnNumericData::kBoolean, 1.0 );
        status = addAttribute( inUseDensityCompensation );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inSubdivideVoxel
    {
        MFnNumericAttribute fnNumericAttribute;
        inSubdivideVoxel = fnNumericAttribute.create( "inSubdivideVoxel", "subdivide", MFnNumericData::kBoolean, 0 );
        status = addAttribute( inSubdivideVoxel );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inNumSubdivisions
    {
        MFnNumericAttribute fnNumericAttribute;
        inNumSubdivisions = fnNumericAttribute.create( "inNumSubdivisions", "subdivisions", MFnNumericData::kInt, 1 );
        fnNumericAttribute.setMin( 1 );
        status = addAttribute( inNumSubdivisions );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inRandomJitter
    {
        MFnNumericAttribute fnNumericAttribute;
        inRandomJitter = fnNumericAttribute.create( "inRandomJitter", "randomJitter", MFnNumericData::kBoolean, 0 );
        status = addAttribute( inRandomJitter );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inJitterWellDistributed
    {
        MFnNumericAttribute fnNumericAttribute;
        inJitterWellDistributed = fnNumericAttribute.create( "inJitterWellDistributed", "jitterWellDistributed",
                                                             MFnNumericData::kBoolean, 0 );
        status = addAttribute( inJitterWellDistributed );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inJitterMultiplePerRegion
    {
        MFnNumericAttribute fnNumericAttribute;
        inJitterMultiplePerVoxel = fnNumericAttribute.create( "inJitterMultiplePerVoxel", "jitterMultiplePerVoxel",
                                                              MFnNumericData::kBoolean, 0 );
        status = addAttribute( inJitterMultiplePerVoxel );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inJitterCountPerRegion
    {
        MFnNumericAttribute fnNumericAttribute;
        inJitterCountPerVoxel =
            fnNumericAttribute.create( "inJitterCountPerVoxel", "jitterPerVoxel", MFnNumericData::kInt, 2 );
        status = addAttribute( inJitterCountPerVoxel );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inRandomSeed
    {
        MFnNumericAttribute fnNumericAttribute;
        inRandomSeed = fnNumericAttribute.create( "inRandomSeed", "randomSeed", MFnNumericData::kInt, 42 );
        status = addAttribute( inRandomSeed );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inNumDistinctRandomValues
    {
        MFnNumericAttribute fnNumericAttribute;
        inNumDistinctRandomValues = fnNumericAttribute.create( "inNumDistinctRandomValues", "numDistinctRandomValues",
                                                               MFnNumericData::kInt, 1024 );
        status = addAttribute( inNumDistinctRandomValues );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inEnableInViewport
    {
        MFnNumericAttribute fnNumericAttribute;
        inEnableInViewport =
            fnNumericAttribute.create( "inEnableInViewport", "enabledViewport", MFnNumericData::kBoolean, 1 );
        status = addAttribute( inEnableInViewport );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inViewportDisableSubdivision
    {
        MFnNumericAttribute fnNumericAttribute;
        inViewportDisableSubdivision = fnNumericAttribute.create(
            "inViewportDisableSubdivision", "disableViewportSubdivision", MFnNumericData::kBoolean, 1 );
        status = addAttribute( inViewportDisableSubdivision );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inViewportParticlePercent
    {
        MFnNumericAttribute fnNumericAttribute;
        inViewportParticlePercent =
            fnNumericAttribute.create( "inViewportParticlePercent", "viewportPercent", MFnNumericData::kFloat, 100.0 );
        fnNumericAttribute.setMin( 0.0 );
        fnNumericAttribute.setMax( 100.0 );
        status = addAttribute( inViewportParticlePercent );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inEnableViewportParticleLimit
    {
        MFnNumericAttribute fnNumericAttribute;
        inEnableViewportParticleLimit = fnNumericAttribute.create(
            "inEnableViewportParticleLimit", "enableViewportParticleLimit", MFnNumericData::kBoolean, 1 );
        status = addAttribute( inEnableViewportParticleLimit );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inViewportParticleLimit
    {
        MFnNumericAttribute fnNumericAttribute;
        inViewportParticleLimit =
            fnNumericAttribute.create( "inViewportParticleLimit", "viewportLimit", MFnNumericData::kFloat, 1000.0 );
        fnNumericAttribute.setMin( 0.0 );
        status = addAttribute( inViewportParticleLimit );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inViewportDisplayMode
    {
        MFnEnumAttribute fnEnumAttribute;
        inViewportDisplayMode =
            fnEnumAttribute.create( "inViewportDisplayMode", "displayMode", PRTObject::DISPLAYMODE_DOT2 );
        fnEnumAttribute.addField( "Display As Small Dots", DISPLAYMODE_DOT1 );
        fnEnumAttribute.addField( "Display As Large Dots", DISPLAYMODE_DOT2 );
        fnEnumAttribute.addField( "Display Normals", DISPLAYMODE_NORMAL );
        fnEnumAttribute.addField( "Display Velocity", DISPLAYMODE_VELOCITY );
        status = addAttribute( inViewportDisplayMode );
    }
    // inUseSurfaceShell
    {
        MFnNumericAttribute fnNumericAttribute;
        inUseSurfaceShell =
            fnNumericAttribute.create( "inUseSurfaceShell", "useSurfaceShell", MFnNumericData::kBoolean, 0 );
        status = addAttribute( inUseSurfaceShell );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inSurfaceShellStart
    {
        MFnNumericAttribute fnNumericAttribute;
        inSurfaceShellStart =
            fnNumericAttribute.create( "inSurfaceShellStart", "shellStart", MFnNumericData::kFloat, 0.0 );
        fnNumericAttribute.setMin( 0.0 );
        status = addAttribute( inSurfaceShellStart );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inSurfaceShellThickness
    {
        MFnNumericAttribute fnNumericAttribute;
        inSurfaceShellThickness =
            fnNumericAttribute.create( "inSurfaceShellThickness", "shellThickness", MFnNumericData::kFloat, 5.0 );
        fnNumericAttribute.setMin( 0.0 );
        status = addAttribute( inSurfaceShellThickness );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outViewportMeshLevelSetProxy
    {
        MFnNumericAttribute fnNumericAttribute;
        outViewportMeshLevelSetProxy =
            fnNumericAttribute.create( "outViewportMeshLevelSetProxy", "viewLevelSetProxy", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        status = addAttribute( outViewportMeshLevelSetProxy );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outViewportLevelSetSamplerProxy
    {
        MFnNumericAttribute fnNumericAttribute;
        outViewportLevelSetSamplerProxy = fnNumericAttribute.create(
            "outViewportLevelSetSamplerProxy", "viewLevelSetSamplerProxy", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        status = addAttribute( outViewportLevelSetSamplerProxy );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outViewportLevelSetSamplerSubdivisionsProxy
    {
        MFnNumericAttribute fnNumericAttribute;
        outViewportLevelSetSamplerSubdivisionsProxy =
            fnNumericAttribute.create( "outViewportLevelSetSamplerSubdivisionsProxy",
                                       "viewLevelSetSamplerSubdivisionsProxy", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        status = addAttribute( outViewportLevelSetSamplerSubdivisionsProxy );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outViewportParticleCacheProxy
    {
        MFnNumericAttribute fnNumericAttribute;
        outViewportParticleCacheProxy = fnNumericAttribute.create( "outViewportParticleCacheProxy",
                                                                   "viewParticleCacheProxy", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        status = addAttribute( outViewportParticleCacheProxy );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inCurrentTransform
    {
        MFnTypedAttribute fnTypedAttribute;
        inCurrentTransform = fnTypedAttribute.create( "inCurrentTransform", "currXForm", MFnData::kMatrix );
        status = addAttribute( inCurrentTransform );
        fnTypedAttribute.setHidden( true );
        fnTypedAttribute.setStorable( false );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    // outViewportMeshLevelSetProxy
    {
        MFnNumericAttribute fnNumericAttribute;
        outRenderMeshLevelSetProxy =
            fnNumericAttribute.create( "outRenderMeshLevelSetProxy", "renderLevelSetProxy", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        // fnNumericAttribute.setWritable( false );
        status = addAttribute( outRenderMeshLevelSetProxy );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outViewportLevelSetSamplerProxy
    {
        MFnNumericAttribute fnNumericAttribute;
        outRenderLevelSetSamplerProxy = fnNumericAttribute.create(
            "outRenderLevelSetSamplerProxy", "renderLevelSetSamplerProxy", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        // fnNumericAttribute.setWritable( false );
        status = addAttribute( outRenderLevelSetSamplerProxy );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outRenderLevelSetSamplerSubdivisionsProxy
    {
        MFnNumericAttribute fnNumericAttribute;
        outRenderLevelSetSamplerSubdivisionsProxy =
            fnNumericAttribute.create( "outRenderLevelSetSamplerSubdivisionsProxy",
                                       "renderLevelSetSamplerSubdivisionsProxy", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        // fnNumericAttribute.setWritable( false );
        status = addAttribute( outRenderLevelSetSamplerSubdivisionsProxy );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    // set up the attribute affect relationships between all the attributes (i.e. to force viewport preview updates)
    static MObject s_meshDependencyAttributes[] = {
        inMesh,
        inRenderSpacing,
        inMeshTransform,
        inUseWorldSpace,
    };

    static MObject s_meshViewportDependencyAttributes[] = {
        inUseCustomViewportSpacing,
        inViewportSpacing,
    };

    for( size_t i = 0; i < sizeof( s_meshDependencyAttributes ) / sizeof( MObject ); ++i ) {
        attributeAffects( s_meshDependencyAttributes[i], outViewportMeshLevelSetProxy );
        attributeAffects( s_meshDependencyAttributes[i], outViewportParticleCacheProxy );

        attributeAffects( s_meshDependencyAttributes[i], outRenderMeshLevelSetProxy );

        register_viewport_dependency( viewportInputDependencies, s_meshDependencyAttributes[i], outParticleStream );
    }

    for( size_t i = 0; i < sizeof( s_meshViewportDependencyAttributes ) / sizeof( MObject ); ++i ) {
        attributeAffects( s_meshViewportDependencyAttributes[i], outViewportMeshLevelSetProxy );
        attributeAffects( s_meshViewportDependencyAttributes[i], outViewportParticleCacheProxy );

        register_viewport_dependency( viewportInputDependencies, s_meshViewportDependencyAttributes[i],
                                      outParticleStream );
    }

    static MObject s_levelSetSamplerDependencyAttributes[] = {
        inRandomJitter, inJitterWellDistributed,   inJitterMultiplePerVoxel, inJitterCountPerVoxel,
        inRandomSeed,   inNumDistinctRandomValues, inUseWorldSpace,
    };

    for( size_t i = 0; i < sizeof( s_levelSetSamplerDependencyAttributes ) / sizeof( MObject ); ++i ) {
        attributeAffects( s_levelSetSamplerDependencyAttributes[i], outViewportLevelSetSamplerProxy );
        attributeAffects( s_levelSetSamplerDependencyAttributes[i], outViewportParticleCacheProxy );

        attributeAffects( s_levelSetSamplerDependencyAttributes[i], outRenderLevelSetSamplerProxy );

        register_viewport_dependency( viewportInputDependencies, s_levelSetSamplerDependencyAttributes[i],
                                      outParticleStream );
    }

    attributeAffects( inEnableInViewport, outViewportLevelSetSamplerProxy );
    attributeAffects( inEnableInViewport, outViewportParticleCacheProxy );
    register_viewport_dependency( viewportInputDependencies, inEnableInViewport, outParticleStream );

    attributeAffects( inUseWorldSpace, outViewportMeshLevelSetProxy );
    attributeAffects( inUseWorldSpace, outRenderMeshLevelSetProxy );
    attributeAffects( inUseWorldSpace, outViewportParticleCacheProxy );
    register_viewport_dependency( viewportInputDependencies, inUseWorldSpace, outParticleStream );

    attributeAffects( inCurrentTransform, outViewportParticleCacheProxy );
    register_viewport_dependency( viewportInputDependencies, inCurrentTransform, outParticleStream );

    static MObject s_levelSetSamplerSubdivisionsDependencyAttributes[] = {
        inSubdivideVoxel,
        inNumSubdivisions,
    };

    for( size_t i = 0; i < sizeof( s_levelSetSamplerSubdivisionsDependencyAttributes ) / sizeof( MObject ); ++i ) {
        attributeAffects( s_levelSetSamplerSubdivisionsDependencyAttributes[i],
                          outViewportLevelSetSamplerSubdivisionsProxy );
        attributeAffects( s_levelSetSamplerSubdivisionsDependencyAttributes[i], outViewportParticleCacheProxy );

        attributeAffects( s_levelSetSamplerSubdivisionsDependencyAttributes[i],
                          outRenderLevelSetSamplerSubdivisionsProxy );
        register_viewport_dependency( viewportInputDependencies, s_levelSetSamplerDependencyAttributes[i],
                                      outParticleStream );
    }

    attributeAffects( inViewportDisableSubdivision, outViewportLevelSetSamplerSubdivisionsProxy );
    attributeAffects( inViewportDisableSubdivision, outViewportParticleCacheProxy );
    register_viewport_dependency( viewportInputDependencies, inViewportDisableSubdivision, outParticleStream );

    static MObject s_particleCacheDependencyAttributes[] = {
        inViewportParticlePercent, inEnableViewportParticleLimit, inViewportParticleLimit,
        inUseSurfaceShell,         inSurfaceShellStart,           inSurfaceShellThickness,
        inEnableInViewport,        outViewportMeshLevelSetProxy,  outViewportLevelSetSamplerProxy,
    };

    for( size_t i = 0; i < sizeof( s_particleCacheDependencyAttributes ) / sizeof( MObject ); ++i ) {
        attributeAffects( s_particleCacheDependencyAttributes[i], outViewportParticleCacheProxy );
        register_viewport_dependency( viewportInputDependencies, s_particleCacheDependencyAttributes[i],
                                      outParticleStream );
    }

    viewportInputDependencies.insert( std::string( MFnAttribute( inViewportDisplayMode ).name().asChar() ) );

    return MStatus::kSuccess;
}

void PRTVolume::postConstructor() {
    PRTObject::postConstructor();
    MStatus stat;

    // Initialize output particle stream
    {
        // prepare the default data
        MFnPluginData fnData;
        MObject pluginMpxData = fnData.create( MTypeId( frantic::maya::MPxParticleStream::id ), &stat );
        if( stat != MStatus::kSuccess )
            throw std::runtime_error( ( "DEBUG: fnData.create()" + stat.errorString() ).asChar() );
        frantic::maya::MPxParticleStream* mpxData = frantic::maya::mpx_cast<MPxParticleStream*>( fnData.data( &stat ) );
        if( !mpxData || stat != MStatus::kSuccess )
            throw std::runtime_error(
                ( "DEBUG: dynamic_cast<frantic::maya::MPxParticleStream*>(...) " + stat.errorString() ).asChar() );
        mpxData->setParticleSource( this );

        // get plug
        MObject obj = thisMObject();
        MFnDependencyNode depNode( obj, &stat );
        if( stat != MStatus::kSuccess )
            throw std::runtime_error(
                ( "DEBUG: could not get dependencyNode from thisMObject():" + stat.errorString() ).asChar() );

        MPlug plug = depNode.findPlug( "outParticleStream", &stat );
        if( stat != MStatus::kSuccess )
            throw std::runtime_error(
                ( "DEBUG: could not find plug 'outParticleStream' from depNode: " + stat.errorString() ).asChar() );

        // set the default data on the plug
        FF_LOG( debug ) << "PRTVolume::postConstructor(): setValue for outParticleStream" << std::endl;
        plug.setValue( pluginMpxData );
    }

    ////this code does not work?
    //{
    //	MFnDependencyNode depNode( thisMObject(), &stat );
    //	MDGModifier dgMod;
    //	MPlug src = depNode.findPlug( worldMatrix, &stat );
    //	MPlug dest = depNode.findPlug( inCurrentTransform, &stat );
    //	stat = dgMod.connect( src, dest );
    //}
}

void* PRTVolume::creator() { return new PRTVolume; }

MTypeId PRTVolume::typeId = 0x00117484;
MString PRTVolume::typeName = "PRTVolume";

PRTVolume::PRTVolume() {
    cacheBoundingBox();
    m_osxViewport20HackInitialized = false;
}

PRTVolume::~PRTVolume() {}

// inherited from MPxSurfaceShape
MBoundingBox PRTVolume::boundingBox() const {
    // maya won't actually trigger a recompute of these intermediate values unless you manually touch them beforehand
    if( inWorldSpace() ) {
        touchSentinelOutput( inMeshTransform );
        touchSentinelOutput( inCurrentTransform );
    }

    touchSentinelOutput( outViewportMeshLevelSetProxy );
    touchSentinelOutput( outViewportLevelSetSamplerProxy );
    touchSentinelOutput( outViewportLevelSetSamplerSubdivisionsProxy );
    touchSentinelOutput( outViewportParticleCacheProxy );

    return m_boundingBox;
}

/**
 * Recalculates the bounding box for this node based on the cached set of particles
 */
void PRTVolume::cacheBoundingBox() {
    m_boundingBox.clear();
    // make sure the bounding box includes the root object as well
    m_boundingBox.expand( MPoint( -1, -1, -1 ) );
    m_boundingBox.expand( MPoint( 1, 1, 1 ) );

    if( m_cachedParticles.get_channel_map().has_channel( _T("Position") ) ) {
        frantic::channels::channel_const_cvt_accessor<vector3f> posAccessor =
            m_cachedParticles.get_channel_map().get_const_cvt_accessor<vector3f>( _T("Position") );
        for( particle_array::iterator it = m_cachedParticles.begin(); it != m_cachedParticles.end(); ++it ) {
            vector3f position = posAccessor.get( *it );
            m_boundingBox.expand( MPoint( position.x, position.y, position.z ) );
        }
    }
    lastWorldSpaceState = inWorldSpace();
}

frantic::channels::channel_map PRTVolume::get_viewport_channel_map(
    boost::shared_ptr<frantic::particles::streams::particle_istream> inputStream ) const {
    frantic::channels::channel_map cm;
    const channel_map& inputCm = inputStream->get_native_channel_map();

    if( inputCm.has_channel( _T("Position") ) )
        cm.define_channel( _T("Position"), 3, frantic::channels::data_type_float32 );
    if( inputCm.has_channel( _T("Velocity") ) )
        cm.define_channel( _T("Velocity"), 3, frantic::channels::data_type_float16 );
    if( inputCm.has_channel( _T("Normal") ) )
        cm.define_channel( _T("Normal"), 3, frantic::channels::data_type_float16 );
    if( inputCm.has_channel( _T("Color") ) )
        cm.define_channel( _T("Color"), 3, frantic::channels::data_type_float16 );
    cm.end_channel_definition();
    return cm;
}

// caches the particles for viewport display purposes
void PRTVolume::cacheViewportParticles() {
    FF_LOG( debug ) << this->name().asChar() << " Calling Cache Viewport" << std::endl;
    if( !getBooleanAttribute( inEnableInViewport ) ) {
        m_cachedParticles.clear();
        return;
    }

    const MTime currentTime = frantic::maya::maya_util::get_current_time();
    MDGContext currentContext( currentTime );
    frantic::graphics::transform4f tm = getTransform( currentContext );

    MFnDependencyNode fnNode( thisMObject() );
    particle_istream_ptr particleStream =
        PRTObjectBase::getFinalParticleStream( thisMObject(), tm, currentContext, true );

    try {
        channel_map viewportMap = get_viewport_channel_map( particleStream );
        m_cachedParticles.reset( viewportMap );
        m_cachedParticles.insert_particles( particleStream );
    } catch( frantic::logging::progress_cancel_exception& e ) {
        m_cachedParticles.clear();
        MGlobal::displayWarning( e.what() );
    } catch( ... ) {
        m_cachedParticles.clear();
        FF_LOG( warning ) << "Error, could not insert particles from stream into array...";
        throw;
    }
}

particle_istream_ptr PRTVolume::buildParticleStream( level_set_ptr levelSet, level_set_sampler_ptr sampler ) const {
    channel_map lsChannelMap;
    lsChannelMap.define_channel( _T("Position"), 3, frantic::channels::data_type_float32 );
    lsChannelMap.define_channel( _T("Normal"), 3, frantic::channels::data_type_float16 );
    // lsChannelMap.define_channel( _T("Velocity"), 3, frantic::channels::data_type_float16 );
    lsChannelMap.end_channel_definition();

    if( !levelSet || !sampler )
        return particle_istream_ptr( new empty_particle_istream( lsChannelMap ) );

    const float outerDistance = (float)getShellStart();
    const float innerDistance = outerDistance + (float)getShellThickness();
    const bool densityCompensation = useDensityCompensation();

    particle_istream_ptr myStream = particle_istream_ptr( new rle_levelset_particle_istream(
        lsChannelMap, levelSet, sampler, -innerDistance, -outerDistance, densityCompensation ) );
    return myStream;
}

PRTVolume::level_set_ptr PRTVolume::buildLevelSet( MDataBlock& inputDataBlock, bool viewportMode ) const {
    // create progress bar for maya
    maya_progress_bar_interface mayaProgressBar;
    mayaProgressBar.set_num_frames( 1 );
    mayaProgressBar.set_current_frame( 0 );

    // hook up mayaProgressBar with logger;
    krakatoasr::krakatoasr_progress_logger progressLogger( &mayaProgressBar, NULL, &mayaProgressBar );

    // determine voxel spacing.
    double voxelSpacing = ( viewportMode ) ? getViewportSpacing() : getRenderSpacing();
    PRTVolume::level_set_ptr outLevelSet;

    MStatus status;
    MPlug meshPlug( thisMObject(), inMesh );

    mayaProgressBar.begin_display();
    // We are checking if maya wants to cancel immediately if it does we are resetting the display
    // Maya should only want to cancel immediately if a cancel request was sent after we end the display
    if( mayaProgressBar.is_cancelled() ) {
        mayaProgressBar.end_display();
        mayaProgressBar.begin_display();
    }

    try {
        // if the plug is not connected, do not display a mesh
        if( !meshPlug.isConnected() )
            return PRTVolume::level_set_ptr();

        // Grab the mesh from Maya.
        trimesh3 franticMesh;
        frantic::maya::geometry::copy_maya_mesh(
            meshPlug, franticMesh, !viewportMode, true, !viewportMode, true,
            !viewportMode ); // Turn off all the fancy options in viewport mode. The only implication here is modifiers
                             // won't see the velocity/normal channel in the viewport.
        progressLogger.update_progress( 5.0f );

        if( franticMesh.is_empty() ) {
            progressLogger.update_progress( 100.0f );
            return PRTVolume::level_set_ptr();
        }

        if( inWorldSpace() ) {
            bool ok;
            frantic::graphics::transform4f trans = getWorldSpaceTransform( MDGContext::fsNormal, &ok );
            franticMesh.transform( trans );
        }

        // Create and return the levelset generated from the mesh.
        progressLogger.push_progress( 5.0f, 100.0f );
        outLevelSet = krakatoa::get_particle_volume_levelset( franticMesh, (float)voxelSpacing, progressLogger );
        progressLogger.pop_progress();

    } catch( frantic::logging::progress_cancel_exception& e ) {
        MGlobal::displayWarning( e.what() );
    }
    mayaProgressBar.end_display();

    return outLevelSet;
}

const frantic::geometry::trimesh3& PRTVolume::getRootMesh() const { return get_prt_volume_icon_mesh(); }

PRTVolume::level_set_sampler_ptr PRTVolume::buildLevelSetSampler( bool viewportMode ) const {
    const unsigned int subdivisionCount = viewportMode ? getViewportSubdivisionCount() : getRenderSubdivisionCount();
    const bool doJitter = isJitterEnabled();
    const unsigned int numPerVoxel = viewportMode ? getViewportJitterPerVoxelCount() : getRenderJitterPerVoxelCount();
    const unsigned int randomSeed = getRandomSeed();
    const unsigned int randomCount = getRandomCount();
    const bool jitterWellDistributed = isJitterWellDistributed();
    return krakatoa::get_particle_volume_voxel_sampler( subdivisionCount, doJitter, numPerVoxel, randomSeed,
                                                        randomCount, jitterWellDistributed );
}

bool PRTVolume::isBounded() const {
    // I'm just not going to bother with trying to compute the bounding box correctly in world-space, right now, its
    // just too weird, plus I can't figure out how to trigger a change on this node's worldspace matrix
    return !inWorldSpace();
}

frantic::graphics::transform4f PRTVolume::getWorldSpaceTransform( const MDGContext& context, bool* isOK ) const {
    MPlug matrixPlug( thisMObject(), inMeshTransform );

    MObject matrixObject;
    MStatus status = matrixPlug.getValue( matrixObject, const_cast<MDGContext&>( context ) );
    if( status ) {

        MFnMatrixData worldMatrixData( matrixObject, &status );
        if( status ) {

            MMatrix worldMatrix = worldMatrixData.matrix( &status );
            if( status ) {

                frantic::graphics::transform4f outTransform = frantic::maya::from_maya_t( worldMatrix );
                if( isOK != NULL )
                    ( *isOK ) = true;
                return outTransform;
            }
        }
    }

    if( isOK != NULL )
        ( *isOK ) = false;
    frantic::graphics::transform4f emptyTrans;
    return emptyTrans;
}

MStatus PRTVolume::setDependentsDirty( const MPlug& plug, MPlugArray& plugArray ) {
    // NOTE: This function does not appear to ever be called in OS X when viewport 2.0 is enabled. Thus we have the
    // update hack in PRTObject.
    MObject thisObj = thisMObject();
    check_dependents_dirty( viewportInputDependencies, plug, thisObj );
    return MStatus::kSuccess;
}

// inherited from MPxNode
MStatus PRTVolume::compute( const MPlug& plug, MDataBlock& block ) {
    if( !m_osxViewport20HackInitialized ) {
        m_osxViewport20HackInitialized = true;
        // Can't be called in PostConstructor because this->name() returns empty string there.
        register_osx_viewport20_update_hack( name(), viewportInputDependencies );
    }

    MStatus status = PRTObject::compute( plug, block );
    try {
        if( plug == outParticleStream ) {
            block.setClean( plug );
            MDataHandle outputData = block.outputValue( outParticleStream );

            MFnPluginData fnData;
            MObject pluginMpxData = fnData.create( MTypeId( frantic::maya::MPxParticleStream::id ), &status );
            if( status != MStatus::kSuccess )
                return status;
            frantic::maya::MPxParticleStream* mpxData =
                frantic::maya::mpx_cast<MPxParticleStream*>( fnData.data( &status ) );
            if( mpxData == NULL )
                return MS::kFailure;
            mpxData->setParticleSource( this );

            outputData.set( mpxData );
            status = MS::kSuccess;
        } else if( plug == outViewportParticleCacheProxy ) {
            block.setClean( plug );
            cacheViewportParticles();
            cacheBoundingBox();
            status = MStatus::kSuccess;
        } else if( plug == outViewportMeshLevelSetProxy ) {
            block.setClean( plug );
            m_cachedMeshLevelSet = buildLevelSet( block, true );
            status = MStatus::kSuccess;
        } else if( plug == outViewportLevelSetSamplerProxy ) {
            block.setClean( plug );
            m_cachedLevelSetSampler = buildLevelSetSampler( true );
            status = MStatus::kSuccess;
        } else if( plug == outViewportLevelSetSamplerSubdivisionsProxy ) {
            block.setClean( plug );
            m_cachedLevelSetSampler->set_subdivs( getViewportSubdivisionCount(), getViewportJitterPerVoxelCount() );
            status = MStatus::kSuccess;
        } else if( plug == outRenderMeshLevelSetProxy ) {
            block.setClean( plug );
            m_cachedRenderMeshLevelSet = buildLevelSet( block, false );
            status = MStatus::kSuccess;
        } else if( plug == outRenderLevelSetSamplerProxy ) {
            block.setClean( plug );
            m_cachedRenderLevelSetSampler = buildLevelSetSampler( false );
            status = MStatus::kSuccess;
        } else if( plug == outRenderLevelSetSamplerSubdivisionsProxy ) {
            block.setClean( plug );
            m_cachedRenderLevelSetSampler->set_subdivs( getRenderSubdivisionCount(), getRenderJitterPerVoxelCount() );
            status = MStatus::kSuccess;
        } else {
            status = MStatus::kUnknownParameter;
        }
    } catch( std::exception& e ) {
        MGlobal::displayError( e.what() );
        return MStatus::kFailure;
    }
    return status;
}

particle_istream_ptr PRTVolume::getRenderParticleStream( const frantic::graphics::transform4f& tm,
                                                         const MDGContext& currentContext ) const {
    MTime contextTime;
    currentContext.getTime( contextTime );
    MTime currentTime = MAnimControl::currentTime();

    // if the evaluation time matches the current scene time, use the internal compute cache
    particle_istream_ptr particleStream;
    if( contextTime == currentTime ) {
        touchSentinelOutput( outRenderMeshLevelSetProxy );
        touchSentinelOutput( outRenderLevelSetSamplerProxy );
        touchSentinelOutput( outRenderLevelSetSamplerSubdivisionsProxy );

        particleStream = buildParticleStream( m_cachedRenderMeshLevelSet, m_cachedRenderLevelSetSampler );
    }
    // otherwise, we must recompute from scratch
    else {
        MDataBlock cachedBlock =
            const_cast<PRTVolume*>( this )->forceCache( const_cast<MDGContext&>( currentContext ) );
        level_set_ptr levelSet = buildLevelSet( cachedBlock, false );
        level_set_sampler_ptr levelSetSampler = buildLevelSetSampler( false );

        particleStream = buildParticleStream( levelSet, levelSetSampler );
    }

    if( inWorldSpace() ) {
        particleStream = boost::shared_ptr<particle_istream>(
            new transformed_particle_istream<float>( particleStream, tm.to_inverse() ) );
    }

    return particleStream;
}

particle_istream_ptr PRTVolume::getViewportParticleStream( const frantic::graphics::transform4f& tm,
                                                           const MDGContext& currentContext ) const {
    particle_istream_ptr particleStream = buildParticleStream( m_cachedMeshLevelSet, m_cachedLevelSetSampler );
    particleStream = apply_fractional_particle_istream( particleStream, getViewportParticleFraction(),
                                                        getViewportParticleLimit(), true );

    if( inWorldSpace() ) {
        particleStream = boost::shared_ptr<particle_istream>(
            new transformed_particle_istream<float>( particleStream, tm.to_inverse() ) );
    }

    return particleStream;
}

frantic::particles::particle_array* PRTVolume::getCachedViewportParticles() {
    // maya won't actually trigger a recompute of these intermediate values unless you manually touch them beforehand
    touchSentinelOutput( outViewportMeshLevelSetProxy );
    touchSentinelOutput( outViewportLevelSetSamplerProxy );
    touchSentinelOutput( outViewportLevelSetSamplerSubdivisionsProxy );
    touchSentinelOutput( outViewportParticleCacheProxy );
    return &m_cachedParticles;
}

PRTObject::display_mode_t PRTVolume::getViewportDisplayMode() {
    return (PRTObject::display_mode_t)getIntAttribute( inViewportDisplayMode );
}

bool PRTVolume::inWorldSpace() const { return getBooleanAttribute( inUseWorldSpace ); }

double PRTVolume::getViewportSpacing() const {
    bool customViewportSpacing = getBooleanAttribute( inUseCustomViewportSpacing );
    return customViewportSpacing ? getDoubleAttribute( inViewportSpacing ) : getRenderSpacing();
}

double PRTVolume::getRenderSpacing() const { return getDoubleAttribute( inRenderSpacing ); }

int PRTVolume::getViewportSubdivisionCount() const {
    bool viewportDisabledSubdivs = getBooleanAttribute( inViewportDisableSubdivision );
    return !viewportDisabledSubdivs ? getRenderSubdivisionCount() : 0;
}

int PRTVolume::getRenderSubdivisionCount() const {
    bool enableSubdivisions = getBooleanAttribute( inSubdivideVoxel );
    return enableSubdivisions ? getIntAttribute( inNumSubdivisions ) : 0;
}

bool PRTVolume::isJitterEnabled() const { return getBooleanAttribute( inRandomJitter ); }

bool PRTVolume::isMultipleJitterEnabled() const { return getBooleanAttribute( inJitterMultiplePerVoxel ); }

int PRTVolume::getViewportJitterPerVoxelCount() const {
    bool viewportDisabledSubdivs = getBooleanAttribute( inViewportDisableSubdivision );
    return !viewportDisabledSubdivs ? getRenderJitterPerVoxelCount() : 1;
}

int PRTVolume::getRenderJitterPerVoxelCount() const {
    return isJitterEnabled() && isMultipleJitterEnabled() ? getIntAttribute( inJitterCountPerVoxel ) : 1;
}

int PRTVolume::getRandomSeed() const { return getIntAttribute( inRandomSeed ); }

int PRTVolume::getRandomCount() const { return getIntAttribute( inNumDistinctRandomValues ); }

bool PRTVolume::isJitterWellDistributed() const { return getBooleanAttribute( inJitterWellDistributed ); }

double PRTVolume::getShellStart() const {
    bool useSurfaceShell = getBooleanAttribute( inUseSurfaceShell );
    return useSurfaceShell ? getDoubleAttribute( inSurfaceShellStart ) : 0.0;
}

double PRTVolume::getShellThickness() const {
    bool useSurfaceShell = getBooleanAttribute( inUseSurfaceShell );
    return useSurfaceShell ? getDoubleAttribute( inSurfaceShellThickness ) : std::numeric_limits<float>::max();
}

bool PRTVolume::useDensityCompensation() const { return getBooleanAttribute( inUseDensityCompensation ); }

double PRTVolume::getViewportParticleFraction() const {
    return getDoubleAttribute( inViewportParticlePercent ) / 100.0;
}

boost::int64_t PRTVolume::getViewportParticleLimit() const {
    const bool enableLimit = getBooleanAttribute( inEnableViewportParticleLimit );
    return enableLimit ? ( static_cast<boost::int64_t>( getDoubleAttribute( inViewportParticleLimit ) * 1000.0 ) )
                       : std::numeric_limits<boost::int64_t>::max();
}

int PRTVolume::getIntAttribute( MObject attribute, MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asInt( context, outStatus );
}

double PRTVolume::getDoubleAttribute( MObject attribute, MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asDouble( context, outStatus );
}

bool PRTVolume::getBooleanAttribute( MObject attribute, MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asBool( context, outStatus );
}

void PRTVolume::touchSentinelOutput( MObject sentinelAttribute ) const {
    MStatus status;
    int value = 0;
    // trying to grab the value from the sentinel plug triggers an update if
    // any of the input plugs are dirty.
    MPlug SentinelPlug( thisMObject(), sentinelAttribute );
    status = SentinelPlug.getValue( value );

    if( SentinelPlug.isArray() && SentinelPlug.numElements() > 0 ) {
        for( unsigned int i = 0; i < SentinelPlug.numElements(); i++ ) {
            MPlug sentinelElement = SentinelPlug.elementByLogicalIndex( i );
            status = sentinelElement.getValue( value );
        }
    }
}
