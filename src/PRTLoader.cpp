// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <math.h>
#include <stdlib.h>

#include <boost/make_shared.hpp>
#include <boost/scoped_array.hpp>

#include "PRTLoader.hpp"
#include "PRTLoaderIconMesh.hpp"
#include "maya_ksr.hpp"
#include <frantic/maya/maya_util.hpp>
#include <frantic/maya/particles/particles.hpp>
#include <frantic/maya/util.hpp>
#include <krakatoa/prt_stream_builder.hpp>

#include <maya/MDoubleArray.h>
#include <maya/MFnPluginData.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MVectorArray.h>

#include <maya/MDGModifier.h>
#include <maya/MDagPathArray.h>
#include <maya/MFileObject.h>
#include <maya/MFloatArray.h>
#include <maya/MFnArrayAttrsData.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

#include <frantic/files/filename_sequence.hpp>
#include <frantic/files/paths.hpp>
#include <frantic/graphics/transform4f.hpp>
#include <frantic/particles/particle_file_stream_factory.hpp>
#include <frantic/particles/streams/concatenated_parallel_particle_istream.hpp>
#include <frantic/particles/streams/concatenated_particle_istream.hpp>
#include <frantic/particles/streams/empty_particle_istream.hpp>
#include <frantic/particles/streams/time_interpolation_particle_istream.hpp>

#include <cmath>
#include <limits>

using namespace frantic;
using namespace frantic::channels;
using namespace frantic::geometry;
using namespace frantic::graphics;
using namespace frantic::particles;
using namespace frantic::particles::streams;
using namespace frantic::maya;

MTypeId PRTLoader::typeId( 0x00117483 );
MString PRTLoader::typeName = "PRTLoader";
const MString PRTLoader::drawRegistrantId( "PrtLoaderPlugin" );

// general
MObject PRTLoader::inTime;
MObject PRTLoader::outSentinel;
MObject PRTLoader::outSingleSimpleRenderFile;
MObject PRTLoader::outNumParticlesOnDisk;
MObject PRTLoader::outNumParticlesInRender;
MObject PRTLoader::outNumParticlesInViewport;

// file loading
MObject PRTLoader::inUseFileInViewport;
MObject PRTLoader::inUseFileInRender;
MObject PRTLoader::inSingleFileOnly;
MObject PRTLoader::inPRTFile;
MObject PRTLoader::inInputFiles;

// timing
MObject PRTLoader::inKeepVelocityChannel;
MObject PRTLoader::inInterpolateSubFrames;
MObject PRTLoader::inPlaybackGraph;
MObject PRTLoader::inEnablePlaybackGraph;
MObject PRTLoader::inFrameOffset;
MObject PRTLoader::inUseCustomRange;
MObject PRTLoader::inCustomRangeStart;
MObject PRTLoader::inCustomRangeEnd;
MObject PRTLoader::inCustomRangeStartClampMode;
MObject PRTLoader::inCustomRangeEndClampMode;

// viewport options
MObject PRTLoader::inViewportLoadMode;
MObject PRTLoader::inViewportParticleLimit;
MObject PRTLoader::inEnableViewportParticleLimit;
MObject PRTLoader::inViewportParticlePercent;
MObject PRTLoader::inViewportDisplayMode;

// render options
MObject PRTLoader::inRenderLoadMode;
MObject PRTLoader::inRenderParticleLimit;
MObject PRTLoader::inEnableRenderParticleLimit;
MObject PRTLoader::inRenderParticlePercent;

// Output Particles
MObject PRTLoader::outParticleStream;

boost::unordered_set<std::string> PRTLoader::viewportInputDependencies;

PRTLoader::PRTLoader() {
    cacheBoundingBox();
    m_osxViewport20HackInitialized = false;
}

PRTLoader::~PRTLoader() {}

void* PRTLoader::creator() { return new PRTLoader; }

MStatus PRTLoader::initialize() {
    MStatus status;
    MFnStringData fnStringData;
    MObject emptyStringObject = fnStringData.create( "" );

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

    // inUseFileInViewport
    {
        MFnNumericAttribute fnNumericAttribute;
        inUseFileInViewport =
            fnNumericAttribute.create( "inUseFileInViewport", "inViewport", MFnNumericData::kBoolean, 1 );
        fnNumericAttribute.setConnectable( false );
        status = addAttribute( inUseFileInViewport );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inUseFileInRender
    {
        MFnNumericAttribute fnNumericAttribute;
        inUseFileInRender = fnNumericAttribute.create( "inUseFileInRender", "inRender", MFnNumericData::kBoolean, 1 );
        fnNumericAttribute.setConnectable( false );
        status = addAttribute( inUseFileInRender );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inSingleFileOnly
    {
        MFnNumericAttribute fnNumericAttribute;
        inSingleFileOnly = fnNumericAttribute.create( "inSingleFileOnly", "singleFile", MFnNumericData::kBoolean, 1 );
        fnNumericAttribute.setConnectable( false );
        status = addAttribute( inSingleFileOnly );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inPRTFile
    {
        MFnTypedAttribute fnTypedAttribute;
        inPRTFile = fnTypedAttribute.create( "inPRTFile", "file", MFnData::kString, emptyStringObject );
        fnTypedAttribute.setConnectable( false );
        fnTypedAttribute.setUsedAsFilename( true );
        status = addAttribute( inPRTFile );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inInputFiles
    {
        MFnCompoundAttribute fnCompoundAttribute;
        inInputFiles = fnCompoundAttribute.create( "inInputFiles", "inputFiles" );
        fnCompoundAttribute.addChild( inPRTFile );
        fnCompoundAttribute.addChild( inUseFileInViewport );
        fnCompoundAttribute.addChild( inUseFileInRender );
        fnCompoundAttribute.addChild( inSingleFileOnly );
        fnCompoundAttribute.setArray( true );
        status = addAttribute( inInputFiles );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inTime
    {
        MFnUnitAttribute fnUnitAttribute;
        inTime = fnUnitAttribute.create( "inTime", "time", MFnUnitAttribute::kTime, 0.0 );
        fnUnitAttribute.setHidden( true );
        status = addAttribute( inTime );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inKeepVelocityChannel
    {
        MFnNumericAttribute fnNumericAttribute;
        inKeepVelocityChannel =
            fnNumericAttribute.create( "inKeepVelocityChannel", "keepVelocity", MFnNumericData::kBoolean, 0.0 );
        status = addAttribute( inKeepVelocityChannel );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inInterpolateSubFrames
    {
        MFnNumericAttribute fnNumericAttribute;
        inInterpolateSubFrames =
            fnNumericAttribute.create( "inInterpolateSubFrames", "interpolate", MFnNumericData::kBoolean, 0.0 );
        status = addAttribute( inInterpolateSubFrames );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inPlaybackGraph
    {
        MFnUnitAttribute fnUnitAttribute;
        inPlaybackGraph = fnUnitAttribute.create( "inPlaybackGraph", "playbackGraph", MTime( 0.0, MTime::uiUnit() ) );
        fnUnitAttribute.setKeyable( true );
        status = addAttribute( inPlaybackGraph );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inEnablePlaybackGraph
    {
        MFnNumericAttribute fnNumericAttribute;
        inEnablePlaybackGraph =
            fnNumericAttribute.create( "inEnablePlaybackGraph", "enablePlaybackGraph", MFnNumericData::kBoolean, 0.0 );
        status = addAttribute( inEnablePlaybackGraph );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inFrameOffset
    {
        MFnNumericAttribute fnNumericAttribute;
        inFrameOffset = fnNumericAttribute.create( "inFrameOffset", "frameOffset", MFnNumericData::kInt, 0.0 );
        status = addAttribute( inFrameOffset );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inUseCustomRange
    {
        MFnNumericAttribute fnNumericAttribute;
        inUseCustomRange = fnNumericAttribute.create( "inUseCustomRange", "useRange", MFnNumericData::kBoolean, 0.0 );
        status = addAttribute( inUseCustomRange );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inCustomRangeStart
    {
        MFnNumericAttribute fnNumericAttribute;
        inCustomRangeStart = fnNumericAttribute.create( "inCustomRangeStart", "rangeStart", MFnNumericData::kInt, 0 );
        status = addAttribute( inCustomRangeStart );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inCustomRangeEnd
    {
        MFnNumericAttribute fnNumericAttribute;
        inCustomRangeEnd = fnNumericAttribute.create( "inCustomRangeEnd", "rangeEnd", MFnNumericData::kInt, 100 );
        status = addAttribute( inCustomRangeEnd );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inCustomRangeStartClampMode
    {
        MFnEnumAttribute fnEnumAttribute;
        inCustomRangeStartClampMode =
            fnEnumAttribute.create( "inCustomRangeStartClampMode", "rangeStartMode", krakatoa::clamp_mode::hold );
        fnEnumAttribute.addField( "Hold First", krakatoa::clamp_mode::hold );
        fnEnumAttribute.addField( "Blank", krakatoa::clamp_mode::blank );
        status = addAttribute( inCustomRangeStartClampMode );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inCustomRangeEndClampMode
    {
        MFnEnumAttribute fnEnumAttribute;
        inCustomRangeEndClampMode =
            fnEnumAttribute.create( "inCustomRangeEndClampMode", "rangeEndMode", krakatoa::clamp_mode::hold );
        fnEnumAttribute.addField( "Hold Last", krakatoa::clamp_mode::hold );
        fnEnumAttribute.addField( "Blank", krakatoa::clamp_mode::blank );
        status = addAttribute( inCustomRangeEndClampMode );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inViewportLoadMode
    {
        MFnEnumAttribute fnEnumAttribute;
        inViewportLoadMode = fnEnumAttribute.create( "inViewportLoadMode", "viewportLoad",
                                                     krakatoa::viewport::PARTICLELOAD_USE_RENDER_SETTING );
        fnEnumAttribute.addField( "Load First N Particles", krakatoa::viewport::PARTICLELOAD_FIRST_N );
        fnEnumAttribute.addField( "Load Every Nth Particle", krakatoa::viewport::PARTICLELOAD_EVENLY_DISTRIBUTE );
        fnEnumAttribute.addField( "Load Every Nth By Id", krakatoa::viewport::PARTICLELOAD_EVENLY_DISTRIBUTE_IDS );
        fnEnumAttribute.addField( "Load Using Render Mode", krakatoa::viewport::PARTICLELOAD_USE_RENDER_SETTING );
        status = addAttribute( inViewportLoadMode );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inViewportParticleLimit
    {
        MFnNumericAttribute fnNumericAttribute;
        inViewportParticleLimit =
            fnNumericAttribute.create( "inViewportParticleLimit", "viewportLimit", MFnNumericData::kDouble, 1000.0 );
        fnNumericAttribute.setMin( 0.0 );
        status = addAttribute( inViewportParticleLimit );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inEnableViewportParticleLimit
    {
        MFnNumericAttribute fnNumericAttribute;
        inEnableViewportParticleLimit = fnNumericAttribute.create( "inEnableViewportParticleLimit",
                                                                   "enableViewportLimit", MFnNumericData::kBoolean, 0 );
        status = addAttribute( inEnableViewportParticleLimit );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inViewportParticlePercent
    {
        MFnNumericAttribute fnNumericAttribute;
        inViewportParticlePercent =
            fnNumericAttribute.create( "inViewportParticlePercent", "viewportPercent", MFnNumericData::kDouble, 100.0 );
        fnNumericAttribute.setMin( 0.0 );
        fnNumericAttribute.setMax( 100.0 );
        fnNumericAttribute.setKeyable( true );
        status = addAttribute( inViewportParticlePercent );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inViewportDisplayMode
    {
        MFnEnumAttribute fnEnumAttribute;
        inViewportDisplayMode = fnEnumAttribute.create( "inViewportDisplayMode", "displayMode", DISPLAYMODE_DOT1 );
        fnEnumAttribute.addField( "Display As Small Dots", DISPLAYMODE_DOT1 );
        fnEnumAttribute.addField( "Display As Large Dots", DISPLAYMODE_DOT2 );
        fnEnumAttribute.addField( "Display Velocities", DISPLAYMODE_VELOCITY );
        fnEnumAttribute.addField( "Display Normals", DISPLAYMODE_NORMAL );
        fnEnumAttribute.addField( "Display Tangents", DISPLAYMODE_TANGENT );
        status = addAttribute( inViewportDisplayMode );
    }
    // inRenderLoadMode
    {
        MFnEnumAttribute fnEnumAttribute;
        inRenderLoadMode =
            fnEnumAttribute.create( "inRenderLoadMode", "renderLoad", krakatoa::render::PARTICLELOAD_FIRST_N );
        fnEnumAttribute.addField( "Load First N Particles", krakatoa::render::PARTICLELOAD_FIRST_N );
        fnEnumAttribute.addField( "Load Every Nth Particle", krakatoa::render::PARTICLELOAD_EVENLY_DISTRIBUTE );
        fnEnumAttribute.addField( "Load Every Nth By Id", krakatoa::render::PARTICLELOAD_EVENLY_DISTRIBUTE_IDS );
        status = addAttribute( inRenderLoadMode );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inRenderParticleLimit
    {
        MFnNumericAttribute fnNumericAttribute;
        inRenderParticleLimit =
            fnNumericAttribute.create( "inRenderParticleLimit", "renderLimit", MFnNumericData::kDouble, 1000.0 );
        fnNumericAttribute.setMin( 0.0 );
        status = addAttribute( inRenderParticleLimit );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inEnableRenderParticleLimit
    {
        MFnNumericAttribute fnNumericAttribute;
        inEnableRenderParticleLimit = fnNumericAttribute.create( "inEnableRenderParticleLimit", "enableRenderLimit",
                                                                 MFnNumericData::kBoolean, 0 );
        status = addAttribute( inEnableRenderParticleLimit );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inRenderParticlePercent
    {
        MFnNumericAttribute fnNumericAttribute;
        inRenderParticlePercent =
            fnNumericAttribute.create( "inRenderParticlePercent", "renderPercent", MFnNumericData::kDouble, 100.0 );
        fnNumericAttribute.setMin( 0.0 );
        fnNumericAttribute.setMax( 100.0 );
        fnNumericAttribute.setKeyable( true );
        status = addAttribute( inRenderParticlePercent );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outSentinel
    {
        MFnNumericAttribute fnNumericAttribute;
        outSentinel = fnNumericAttribute.create( "outSentinel", "sentinel", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        fnNumericAttribute.setWritable( false );
        status = addAttribute( outSentinel );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outSingleSimpleRenderFile
    {
        MFnTypedAttribute fnTypedAttribute;
        outSingleSimpleRenderFile = fnTypedAttribute.create( "outSingleSimpleRenderFile", "singleOutputFile",
                                                             MFnData::kString, emptyStringObject );
        fnTypedAttribute.setHidden( true );
        fnTypedAttribute.setStorable( false );
        fnTypedAttribute.setWritable( false );
        status = addAttribute( outSingleSimpleRenderFile );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outNumParticlesOnDisk
    {
        MFnNumericAttribute fnNumericAttribute;
        outNumParticlesOnDisk =
            fnNumericAttribute.create( "outNumParticlesOnDisk", "numDiskParticles", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        fnNumericAttribute.setWritable( false );
        status = addAttribute( outNumParticlesOnDisk );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outNumParticlesInRender
    {
        MFnTypedAttribute fnTypedAttribute;
        outNumParticlesInRender = fnTypedAttribute.create( "outNumParticlesInRender", "numRenderParticles",
                                                           MFnData::kString, emptyStringObject );
        fnTypedAttribute.setHidden( true );
        fnTypedAttribute.setStorable( false );
        fnTypedAttribute.setWritable( false );
        status = addAttribute( outNumParticlesInRender );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // outNumParticlesInViewport
    {
        MFnNumericAttribute fnNumericAttribute;
        outNumParticlesInViewport =
            fnNumericAttribute.create( "outNumParticlesInViewport", "numViewportParticles", MFnNumericData::kInt, 0 );
        fnNumericAttribute.setHidden( true );
        fnNumericAttribute.setStorable( false );
        fnNumericAttribute.setWritable( false );
        status = addAttribute( outNumParticlesInViewport );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    // This is only a code-reduction strategy: because there are so many dependency attributes, most of
    // which fall into common categories, setting them up as lists helps cut down on what would be
    // otherwise an enourmous code wall of 'attributeAffects' calls
    static MObject s_commonDependencyAttributes[] = {
        inTime,
        inInputFiles,
        inKeepVelocityChannel,
        inPlaybackGraph,
        inEnablePlaybackGraph,
        inInterpolateSubFrames,
        inFrameOffset,
        inUseCustomRange,
        inCustomRangeStart,
        inCustomRangeEnd,
        inCustomRangeStartClampMode,
        inCustomRangeEndClampMode,
    };
    static MObject s_viewportDependencyAttributes[] = { inViewportLoadMode,      inEnableViewportParticleLimit,
                                                        inViewportParticleLimit, inViewportParticlePercent,
                                                        inRenderLoadMode,        inRenderParticlePercent };
    static MObject s_renderDependencyAttributes[] = {
        inRenderLoadMode,
        inEnableRenderParticleLimit,
        inRenderParticleLimit,
        inRenderParticlePercent,
    };

    // set up dependencies between all common attributes
    for( size_t i = 0; i < sizeof( s_commonDependencyAttributes ) / sizeof( MObject ); ++i ) {
        register_viewport_dependency( viewportInputDependencies, s_commonDependencyAttributes[i], outSentinel );

        attributeAffects( s_commonDependencyAttributes[i], outSingleSimpleRenderFile );
        attributeAffects( s_commonDependencyAttributes[i], outNumParticlesOnDisk );
        attributeAffects( s_commonDependencyAttributes[i], outNumParticlesInRender );

        attributeAffects( s_commonDependencyAttributes[i], outParticleStream );
    }

    // set up dependencies between viewport attributes
    for( size_t i = 0; i < sizeof( s_viewportDependencyAttributes ) / sizeof( MObject ); ++i ) {
        register_viewport_dependency( viewportInputDependencies, s_viewportDependencyAttributes[i], outSentinel );

        attributeAffects( s_commonDependencyAttributes[i], outParticleStream );
    }

    // set up dependencies between viewport attributes
    for( size_t i = 0; i < sizeof( s_renderDependencyAttributes ) / sizeof( MObject ); ++i ) {
        attributeAffects( s_renderDependencyAttributes[i], outNumParticlesInRender );
        attributeAffects( s_renderDependencyAttributes[i], outSingleSimpleRenderFile );
    }

    // Aren't added above, so must be added explicitly
    viewportInputDependencies.insert( std::string( MFnAttribute( inPRTFile ).name().asChar() ) );
    viewportInputDependencies.insert( std::string( MFnAttribute( inViewportDisplayMode ).name().asChar() ) );
    viewportInputDependencies.insert( std::string( MFnAttribute( inUseFileInViewport ).name().asChar() ) );

    return MS::kSuccess;
}

void PRTLoader::postConstructor() {
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
        FF_LOG( debug ) << "PRTLoader::postConstructor(): setValue for outParticleStream" << std::endl;
        plug.setValue( pluginMpxData );
    }
}

MBoundingBox PRTLoader::boundingBox() const { return m_boundingBox; }

/**
 * Recalculates the bounding box for this node based on the cached set of particles
 */
void PRTLoader::cacheBoundingBox() {
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
}

bool PRTLoader::isBounded() const { return true; }

particle_array* PRTLoader::getCachedViewportParticles() {
    touchSentinelOutput();
    return &m_cachedParticles;
}

PRTObject::display_mode_t PRTLoader::getViewportDisplayMode() {
    return (PRTObject::display_mode_t)getIntAttribute( inViewportDisplayMode );
}

// Performs a simple check to see if the entire output can be represented as a single particle file.
// Note that this makes some simplifying (and thus, incorrect) assumptions: for example, if 'load
// single file' is selected, this will return that file, even if 'keep velocity channel' is not
// also selected, meaning the velocities will be incorrectly preserved rather than zeroed.
bool PRTLoader::isSingleSimpleRenderFile( MTime currentTime, frantic::tstring& outFileName ) const {
    std::vector<input_file_info> inputFiles;
    getInputFiles( inputFiles );
    int renderFileCount = 0;
    int singleFileIndex = -1;

    for( size_t i = 0; i < inputFiles.size(); ++i ) {
        if( inputFiles[i].inRender ) {
            ++renderFileCount;

            if( renderFileCount == 1 ) {
                singleFileIndex = (int)i;
            } else {
                break;
            }
        }
    }

    if( renderFileCount == 1 && !getBooleanAttribute( inEnableRenderParticleLimit ) &&
        getRenderParticleFraction() == 1.0 ) {
        if( inputFiles[singleFileIndex].singleFileOnly ) {
            outFileName = inputFiles[singleFileIndex].baseName;
            return true;
        } else if( !getBooleanAttribute( inEnableRenderParticleLimit ) ) {
            MTime playbackTime = getEffectivePlaybackTime( currentTime );
            int currentFrame = (int)floor( playbackTime.asUnits( MTime::uiUnit() ) + 0.5 );

            if( useCustomRange() ) {
                if( currentFrame < getRangeStart() ) {
                    if( getRangeStartClampMode() == krakatoa::clamp_mode::hold )
                        currentFrame = getRangeStart();
                    else
                        return false;
                } else if( currentFrame > getRangeEnd() ) {
                    if( getRangeEndClampMode() == krakatoa::clamp_mode::hold )
                        currentFrame = getRangeEnd();
                    else
                        return false;
                }
            }

            currentFrame += getFrameOffset();
            files::filename_pattern filePattern( inputFiles[singleFileIndex].baseName );
            outFileName = filePattern[currentFrame];
            return true;
        }
    }

    return false;
}

MStatus PRTLoader::setDependentsDirty( const MPlug& plug, MPlugArray& plugArray ) {
    // NOTE: This function does not appear to ever be called in OS X when viewport 2.0 is enabled. Thus we have the
    // update hack in PRTObject.
    MObject thisObj = thisMObject();
    check_dependents_dirty( viewportInputDependencies, plug, thisObj );
    return MS::kSuccess;
}

MStatus PRTLoader::compute( const MPlug& plug, MDataBlock& block ) {

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
        } else if( plug == outSentinel ) {
            block.setClean( plug );
            MTime currentTime;
            MStringArray inputFiles;
            MDataHandle outputData;
            // get input time
            status = getCurrentTime( block, currentTime );
            CHECK_MSTATUS_AND_RETURN_IT( status );

            // re-compute the cached particle array
            cacheParticlesAt( currentTime );
            cacheBoundingBox();

            outputData = block.outputValue( outNumParticlesInViewport );

            if( m_cachedParticles.get_channel_map().structure_size() == 0 )
                outputData.setInt( 0 );
            else {
                outputData.setInt( (int)m_cachedParticles.particle_count() );
            }

            status = MS::kSuccess;

        } else if( plug == outSingleSimpleRenderFile ) {
            block.setClean( plug );
            MTime currentTime;
            status = getCurrentTime( block, currentTime );
            CHECK_MSTATUS_AND_RETURN_IT( status );
            MDataHandle outputData = block.outputValue( outSingleSimpleRenderFile );
            frantic::tstring singleFile;
            if( isSingleSimpleRenderFile( currentTime, singleFile ) ) {
                outputData.setString( MString( frantic::strings::to_string( singleFile ).c_str() ) );
            } else {
                outputData.setString( MString( "" ) );
            }
            status = MS::kSuccess;
        } else if( plug == outNumParticlesOnDisk ) {
            block.setClean( plug );
            MTime currentTime;
            status = getCurrentTime( block, currentTime );
            CHECK_MSTATUS_AND_RETURN_IT( status );
            MDataHandle outputData = block.outputValue( outNumParticlesOnDisk );

            outputData.setInt( (int)getParticleStream( currentTime, krakatoa::enabled_mode::all )->particle_count() );
            status = MS::kSuccess;
        } else if( plug == outNumParticlesInRender ) {
            block.setClean( plug );
            MTime currentTime;

            status = getCurrentTime( block, currentTime );
            CHECK_MSTATUS_AND_RETURN_IT( status );

            MDataHandle outputData = block.outputValue( outNumParticlesInRender );
            MString output( "" );
            int particleCount = (int)getParticleStream( currentTime, krakatoa::enabled_mode::render )->particle_count();

            if( particleCount == -1 ) {
                outputData.setString( "???" );
            } else {
                output.set( particleCount, 3 );
                outputData.setString( output );
            }

            status = MS::kSuccess;
        }
    } catch( std::exception& e ) {
        MGlobal::displayError( e.what() );
        return MS::kFailure;
    }

    return status;
}

// This method is used by Maya to determine the list of external files to that are used by this node
// (this isn't actually at all correct, since it isn't enumerating the full file sequence, only the
// raw files initially specified)
MStringArray PRTLoader::getFilesToArchive( bool shortName, bool unresolvedName, bool markCouldBeImageSequence ) const {
    std::vector<input_file_info> inputFiles;
    MStatus status = getInputFiles( inputFiles );
    MStringArray outputArray;

    if( status ) {
        // Perform the requested transformations on the list of files
        for( unsigned int i = 0; i < inputFiles.size(); ++i ) {
            MString resultingName = inputFiles[i].baseName.c_str();

            if( !unresolvedName ) {
                MFileObject fileObj;
                fileObj.setRawName( resultingName );
                resultingName = fileObj.resolvedFullName();
            }

            if( shortName ) {
                resultingName = frantic::files::filename_from_path( resultingName.asChar() ).c_str();
            }

            // does this make sense in this context?  How does maya expect to identify elements of a file sequence, or
            // does it only apply to images in the first place
            if( markCouldBeImageSequence && !inputFiles[i].singleFileOnly ) {
                resultingName += "*";
            }

            outputArray.append( resultingName );
        }
    } else {
        outputArray.clear();
    }

    return outputArray;
}

MStatus PRTLoader::getInputFiles( std::vector<input_file_info>& outFiles ) const {
    MStatus status;
    outFiles.clear();
    MPlug plug( thisMObject(), inInputFiles );

    for( unsigned int i = 0; i < plug.numElements(); ++i ) {
        MPlug compoundObject = plug.elementByPhysicalIndex( i, &status );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        MPlug fileNamePlug = compoundObject.child( inPRTFile, &status );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        MPlug singleFileOnlyPlug = compoundObject.child( inSingleFileOnly, &status );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        MPlug renderFilePlug = compoundObject.child( inUseFileInRender, &status );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        MPlug viewportFilePlug = compoundObject.child( inUseFileInViewport, &status );
        CHECK_MSTATUS_AND_RETURN_IT( status );
        outFiles.push_back( input_file_info() );
        outFiles.back().baseName = frantic::strings::to_tstring( fileNamePlug.asString().asChar() );
        outFiles.back().inRender = renderFilePlug.asBool();
        outFiles.back().inViewport = viewportFilePlug.asBool();
        outFiles.back().singleFileOnly = singleFileOnlyPlug.asBool();
    }

    return MStatus::kSuccess;
}

MStatus PRTLoader::getCurrentTime( MDataBlock& block, MTime& outTime ) const {
    MStatus status;
    MDataHandle hValue = block.inputValue( inTime, &status );

    if( status == MS::kSuccess )
        outTime = hValue.asTime();

    return ( status );
}

const frantic::geometry::trimesh3& PRTLoader::getRootMesh() const { return get_prt_loader_icon_mesh(); }

frantic::channels::channel_map PRTLoader::get_viewport_channel_map(
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
    if( inputCm.has_channel( _T("Tangent") ) )
        cm.define_channel( _T("Tangent"), 3, frantic::channels::data_type_float16 );
    cm.end_channel_definition();
    return cm;
}

// caches the particles for viewport display purposes
void PRTLoader::cacheParticlesAt( MTime time ) {
    MDGContext context( time );
    frantic::graphics::transform4f tm = getTransform( time );

    MFnDependencyNode fnNode( thisMObject() );
    boost::shared_ptr<particle_istream> particleStream =
        PRTObjectBase::getFinalParticleStream( fnNode, tm, context, true );

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

/**
 * In order to get Maya to correctly refresh all dirty input values, it is neccessary that it
 * thinks this node is using them to calculate and use some output value.  Hence, this method
 * will re-trigger computation of the cached particle set if any of the input values have changed.
 */
void PRTLoader::touchSentinelOutput() const {
    MStatus status;
    int value = 0;
    // trying the grab the value from the sentinel plug triggers an update if
    // any of the input plugs are dirty.
    MPlug SentinelPlug( thisMObject(), outSentinel );
    SentinelPlug.getValue( value );
}

particle_istream_ptr PRTLoader::getRenderParticleStream( const frantic::graphics::transform4f& tm,
                                                         const MDGContext& currentContext ) const {
    particle_istream_ptr outStream = getParticleStream( currentContext, krakatoa::enabled_mode::render );
    return outStream;
}

particle_istream_ptr PRTLoader::getViewportParticleStream( const frantic::graphics::transform4f& tm,
                                                           const MDGContext& currentContext ) const {
    particle_istream_ptr outStream = getParticleStream( currentContext, krakatoa::enabled_mode::viewport );
    return outStream;
}

/**
 * Primary method for building particle streams from the input parameters.
 *
 * @param atTime the scene time at which to build the particle stream
 * @param mode whether to build the viewport particle set, or the render particle set
 * @return the combined particle stream object
 */
PRTObject::particle_istream_ptr PRTLoader::getParticleStream( const MDGContext& currentContext,
                                                              krakatoa::enabled_mode::enabled_mode_enum mode ) const {
    MTime currentTime;
    currentContext.getTime( currentTime );

    krakatoa::prt_stream_builder builder;
    builder.m_currentTime = getEffectivePlaybackTime( currentTime ).as( MTime::uiUnit() );
    builder.m_framesPerSecond = maya_util::get_fps();
    builder.m_coordinateSystem = frantic::maya::get_coordinate_system();
    builder.m_scaleToMeters = frantic::maya::get_scale_to_meters();
    std::vector<input_file_info> inputFiles;
    getInputFiles( inputFiles );

    for( unsigned int i = 0; i < inputFiles.size(); ++i ) {
        if( mode == krakatoa::enabled_mode::all ||
            ( mode == krakatoa::enabled_mode::viewport && inputFiles[i].inViewport ) ||
            ( mode == krakatoa::enabled_mode::render && inputFiles[i].inRender ) ) {
            if( inputFiles[i].singleFileOnly ) {
                builder.m_singleFiles.push_back( inputFiles[i].baseName );
            } else {
                builder.m_fileSequences.push_back( inputFiles[i].baseName );
            }
        }
    }

    builder.m_frameOffset = getFrameOffset();
    builder.m_interpolateSubFrames = interpolateSubFrames();
    builder.m_keepVelocityChannel = keepVelocityChannel();

    if( useCustomRange() ) {
        builder.m_rangeStart = getRangeStart();
        builder.m_rangeEnd = getRangeEnd();
        builder.m_rangeStartClampMode = getRangeStartClampMode();
        builder.m_rangeEndClampMode = getRangeEndClampMode();
    }

    if( mode == krakatoa::enabled_mode::viewport ) {
        builder.m_loadMode = getEffectiveViewportLoadMode();
        builder.m_loadFraction = getViewportParticleFraction( currentContext );
        builder.m_loadLimit = getViewportParticleLimit();
    } else if( mode == krakatoa::enabled_mode::render ) {
        builder.m_loadMode = getRenderLoadMode();
        builder.m_loadFraction = getRenderParticleFraction( currentContext );
        builder.m_loadLimit = getRenderParticleLimit();
    } else { // mode == enabled_mode::all (used for "on disk" particle count plug).
        builder.m_loadMode = krakatoa::render::PARTICLELOAD_FIRST_N;
        builder.m_loadFraction = 1.0;
        builder.m_loadLimit = std::numeric_limits<boost::int64_t>::max();
    }

    MTime intervalStart = getEffectivePlaybackTime( currentTime - MTime( 0.5, MTime::uiUnit() ) );
    MTime intervalEnd = getEffectivePlaybackTime( currentTime + MTime( 0.5, MTime::uiUnit() ) );
    builder.m_timeDerivative = ( intervalEnd - intervalStart ).as( MTime::uiUnit() );

    particle_istream_ptr outStream = builder.generate_stream();

    return outStream;
}

MTime PRTLoader::getEffectivePlaybackTime( MTime atTime ) const {
    bool playbackGraphEnabled = getBooleanAttribute( inEnablePlaybackGraph );
    MDGContext contex( atTime );
    return playbackGraphEnabled ? getTimeAttribute( inPlaybackGraph, contex ) : atTime;
}

int PRTLoader::getIntAttribute( MObject attribute, const MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asInt( const_cast<MDGContext&>( context ), outStatus );
}

double PRTLoader::getDoubleAttribute( MObject attribute, const MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asDouble( const_cast<MDGContext&>( context ), outStatus );
}

bool PRTLoader::getBooleanAttribute( MObject attribute, const MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asBool( const_cast<MDGContext&>( context ), outStatus );
}

MTime PRTLoader::getTimeAttribute( MObject attribute, const MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asMTime( const_cast<MDGContext&>( context ), outStatus );
}

boost::int64_t PRTLoader::getViewportParticleLimit() const {
    const bool enableLimit = getBooleanAttribute( inEnableViewportParticleLimit );
    return enableLimit ? ( static_cast<boost::int64_t>( getDoubleAttribute( inViewportParticleLimit ) * 1000.0 ) )
                       : std::numeric_limits<boost::int64_t>::max();
}

double PRTLoader::getViewportParticleFraction( const MDGContext& context ) const {
    // This viewport fraction is meant to be a fraction of the render percentage, not of the total available particles
    return getRenderParticleFraction( context ) * ( getDoubleAttribute( inViewportParticlePercent, context ) / 100.0 );
}

krakatoa::viewport::RENDER_LOAD_PARTICLE_MODE PRTLoader::getViewportLoadMode() const {
    return (krakatoa::viewport::RENDER_LOAD_PARTICLE_MODE)getIntAttribute( inViewportLoadMode );
}

krakatoa::render::RENDER_LOAD_PARTICLE_MODE PRTLoader::getEffectiveViewportLoadMode() const {
    krakatoa::viewport::RENDER_LOAD_PARTICLE_MODE viewportMode = getViewportLoadMode();

    if( viewportMode == krakatoa::viewport::PARTICLELOAD_USE_RENDER_SETTING )
        return getRenderLoadMode();
    else
        return krakatoa::get_as_render_mode( viewportMode );
}

boost::int64_t PRTLoader::getRenderParticleLimit() const {
    const bool enableLimit = getBooleanAttribute( inEnableRenderParticleLimit );
    return enableLimit ? ( static_cast<boost::int64_t>( getDoubleAttribute( inRenderParticleLimit ) * 1000.0 ) )
                       : std::numeric_limits<boost::int64_t>::max();
}

double PRTLoader::getRenderParticleFraction( const MDGContext& context ) const {
    return getDoubleAttribute( inRenderParticlePercent, context ) / 100.0;
}

krakatoa::render::RENDER_LOAD_PARTICLE_MODE PRTLoader::getRenderLoadMode() const {
    return (krakatoa::render::RENDER_LOAD_PARTICLE_MODE)getIntAttribute( inRenderLoadMode );
}

int PRTLoader::getFrameOffset() const { return getIntAttribute( inFrameOffset ); }

bool PRTLoader::interpolateSubFrames() const { return getBooleanAttribute( inInterpolateSubFrames ); }

bool PRTLoader::useCustomRange() const { return getBooleanAttribute( inUseCustomRange ); }

bool PRTLoader::keepVelocityChannel() const { return getBooleanAttribute( inKeepVelocityChannel ); }

int PRTLoader::getRangeStart() const { return getIntAttribute( inCustomRangeStart ); }

int PRTLoader::getRangeEnd() const { return getIntAttribute( inCustomRangeEnd ); }

krakatoa::clamp_mode::clamp_mode_enum PRTLoader::getRangeStartClampMode() const {
    return (krakatoa::clamp_mode::clamp_mode_enum)getIntAttribute( inCustomRangeStartClampMode );
}

krakatoa::clamp_mode::clamp_mode_enum PRTLoader::getRangeEndClampMode() const {
    return (krakatoa::clamp_mode::clamp_mode_enum)getIntAttribute( inCustomRangeEndClampMode );
}
