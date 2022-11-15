// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <math.h>
#include <stdlib.h>

#include "PRTFractal.hpp"
#include "PRTFractalsIconMesh.hpp"
#include "maya_ksr.hpp"
#include <frantic/maya/maya_util.hpp>
#include <frantic/maya/particles/particles.hpp>
#include <frantic/maya/util.hpp>

#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MGlobal.h>

#include <maya/MFloatPointArray.h>
#include <maya/MFnMeshData.h>

#include <frantic/graphics/color3f.hpp>
#include <frantic/graphics/quat4f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <krakatoasr_particles.hpp>
#include <krakatoasr_renderer/params.hpp>

#include <cmath>

using namespace frantic;
using namespace frantic::channels;
using namespace frantic::graphics;
using namespace frantic::particles;
using namespace frantic::particles::streams;
using namespace frantic::maya;

const MString PRTFractal::drawRegistrantId( "PrtFractalPlugin" );
const MString PRTFractal::drawClassification( "drawdb/geometry/prtfractal" );

// If MAXTRANFORMATIONS is changed, change again in AddFractalTransformKeyframes.cpp to match
#define MAXTRANSFORMATIONS 10
#define DEFAULTAFFINETRANSFORMATIONS 3
#define DEFAULTRANDOMSEED 4545

MTypeId PRTFractal::typeId( 0x00117485 );
MString PRTFractal::typeName = "PRTFractal";

// general
MObject PRTFractal::inTime;
MObject PRTFractal::outSentinel;

// number of transformations
MObject PRTFractal::inAffineTransformationCount;

// viewport options
MObject PRTFractal::inViewportLoadMode;    // required
MObject PRTFractal::inViewportDisplayMode; // required

// All transformations
MObject PRTFractal::inPosX[MAXTRANSFORMATIONS];
MObject PRTFractal::inPosY[MAXTRANSFORMATIONS];
MObject PRTFractal::inPosZ[MAXTRANSFORMATIONS];
MObject PRTFractal::inRotX[MAXTRANSFORMATIONS];
MObject PRTFractal::inRotY[MAXTRANSFORMATIONS];
MObject PRTFractal::inRotZ[MAXTRANSFORMATIONS];
MObject PRTFractal::inRotW[MAXTRANSFORMATIONS];
MObject PRTFractal::inScaleX[MAXTRANSFORMATIONS];
MObject PRTFractal::inScaleY[MAXTRANSFORMATIONS];
MObject PRTFractal::inScaleZ[MAXTRANSFORMATIONS];
MObject PRTFractal::inSkewX[MAXTRANSFORMATIONS];
MObject PRTFractal::inSkewY[MAXTRANSFORMATIONS];
MObject PRTFractal::inSkewZ[MAXTRANSFORMATIONS];
MObject PRTFractal::inSkewW[MAXTRANSFORMATIONS];
MObject PRTFractal::inSkewA[MAXTRANSFORMATIONS];
MObject PRTFractal::inWeight[MAXTRANSFORMATIONS];

// Start and End Colors of the fractal
MObject PRTFractal::inStartColor;
MObject PRTFractal::inEndColor;

// number of particles in fractal
MObject PRTFractal::inRenderParticleCount;
MObject PRTFractal::inViewportParticleCount;

MObject PRTFractal::inFractalRandomSeed;

// Output Particles
MObject PRTFractal::outParticleStream;

boost::unordered_set<std::string> PRTFractal::viewportInputDependencies;

PRTFractal::PRTFractal() {
    cacheBoundingBox();
    m_osxViewport20HackInitialized = false;
}

PRTFractal::~PRTFractal() {}

void* PRTFractal::creator() { return new PRTFractal; }

MStatus PRTFractal::initialize() {
    MStatus status;
    // MFnCompoundAttribute fnCompoundAttribute;
    // MFnStringData fnStringData;
    // MObject emptyStringObject = fnStringData.create( "" );

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

    // affineTransformationCount
    {
        MFnNumericAttribute fnNumericAttribute;
        inAffineTransformationCount =
            fnNumericAttribute.create( "inAffineTransformationCount", "affineTransformationCount", MFnNumericData::kInt,
                                       DEFAULTAFFINETRANSFORMATIONS );
        fnNumericAttribute.setMin( 2 );
        fnNumericAttribute.setMax( MAXTRANSFORMATIONS );
        fnNumericAttribute.setConnectable( true );
        status = addAttribute( inAffineTransformationCount );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    {
        MFnNumericAttribute fnNumericAttribute;
        inRenderParticleCount =
            fnNumericAttribute.create( "inRenderParticleCount", "renderParticleCount", MFnNumericData::kInt, 1000000 );
        fnNumericAttribute.setMin( 0 );
        status = addAttribute( inRenderParticleCount );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    {
        MFnNumericAttribute fnNumericAttribute;
        inViewportParticleCount = fnNumericAttribute.create( "inViewportParticleCount", "viewportParticleCount",
                                                             MFnNumericData::kInt, 100000 );
        fnNumericAttribute.setMin( 0 );
        status = addAttribute( inViewportParticleCount );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    {
        MFnNumericAttribute fnNumericAttribute;
        inFractalRandomSeed = fnNumericAttribute.create( "inFractalRandomSeed", "fractalRandomSeed",
                                                         MFnNumericData::kInt, DEFAULTRANDOMSEED );
        fnNumericAttribute.setMin( 0 );
        status = addAttribute( inFractalRandomSeed );
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

    krakatoasr::fractal_parameters params;
    params.set_from_random( DEFAULTAFFINETRANSFORMATIONS, 1, DEFAULTRANDOMSEED );
    for( int i = 0; i < MAXTRANSFORMATIONS; i++ ) {
        vector3f positions;
        frantic::graphics::quat4f rotation;
        vector3f scale;
        frantic::graphics::quat4f skew;
        float skewAngle;
        float weight;
        if( i < DEFAULTAFFINETRANSFORMATIONS ) {
            positions = params.get_data()->position[i];
            rotation = params.get_data()->rotation[i];
            scale = params.get_data()->scale[i];
            skew = params.get_data()->skewOrientation[i];
            skewAngle = params.get_data()->skewAngle[i];
            weight = params.get_data()->weight[i];
        } else {
            positions = vector3f( 0, 0, 0 );
            rotation = quat4f( 0, 0, 0, 0 );
            scale = vector3f( 0, 0, 0 );
            skew = quat4f( 0, 0, 0, 0 );
            skewAngle = 0.0;
            weight = 0.0;
        }

        std::string attributeName;
        MFnNumericAttribute fnNumericAttribute;

        attributeName = "inPosX" + boost::lexical_cast<std::string>( i );
        inPosX[i] = fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble,
                                               positions.x );
        fnNumericAttribute.setMin( -1.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inPosX[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inPosY" + boost::lexical_cast<std::string>( i );
        inPosY[i] = fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble,
                                               positions.y );
        fnNumericAttribute.setMin( -1.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inPosY[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inPosZ" + boost::lexical_cast<std::string>( i );
        inPosZ[i] = fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble,
                                               positions.z );
        fnNumericAttribute.setMin( -1.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inPosZ[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inRotX" + boost::lexical_cast<std::string>( i );
        inRotX[i] = fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble,
                                               rotation.x );
        fnNumericAttribute.setMin( -1.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inRotX[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inRotY" + boost::lexical_cast<std::string>( i );
        inRotY[i] = fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble,
                                               rotation.y );
        fnNumericAttribute.setMin( -1.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inRotY[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inRotZ" + boost::lexical_cast<std::string>( i );
        inRotZ[i] = fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble,
                                               rotation.z );
        fnNumericAttribute.setMin( -1.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inRotZ[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inRotW" + boost::lexical_cast<std::string>( i );
        inRotW[i] = fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble,
                                               rotation.w );
        status = addAttribute( inRotW[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inScaleX" + boost::lexical_cast<std::string>( i );
        inScaleX[i] =
            fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble, scale.x );
        fnNumericAttribute.setMin( -10.0 );
        fnNumericAttribute.setMax( 10.0 );
        status = addAttribute( inScaleX[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inScaleY" + boost::lexical_cast<std::string>( i );
        inScaleY[i] =
            fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble, scale.y );
        fnNumericAttribute.setMin( -10.0 );
        fnNumericAttribute.setMax( 10.0 );
        status = addAttribute( inScaleY[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inScaleZ" + boost::lexical_cast<std::string>( i );
        inScaleZ[i] =
            fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble, scale.z );
        fnNumericAttribute.setMin( -10.0 );
        fnNumericAttribute.setMax( 10.0 );
        status = addAttribute( inScaleZ[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inSkewX" + boost::lexical_cast<std::string>( i );
        inSkewX[i] =
            fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble, skew.x );
        fnNumericAttribute.setMin( -1.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inSkewX[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inSkewY" + boost::lexical_cast<std::string>( i );
        inSkewY[i] =
            fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble, skew.y );
        fnNumericAttribute.setMin( -1.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inSkewY[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inSkewZ" + boost::lexical_cast<std::string>( i );
        inSkewZ[i] =
            fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble, skew.z );
        fnNumericAttribute.setMin( -1.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inSkewZ[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inSkewW" + boost::lexical_cast<std::string>( i );
        inSkewW[i] =
            fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble, skew.w );
        status = addAttribute( inSkewW[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inSkewA" + boost::lexical_cast<std::string>( i );
        inSkewA[i] = fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble,
                                                skewAngle );
        fnNumericAttribute.setMin( 0.0 );
        fnNumericAttribute.setMax( 360.0 );
        status = addAttribute( inSkewA[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );

        attributeName = "inWeight" + boost::lexical_cast<std::string>( i );
        inWeight[i] =
            fnNumericAttribute.create( attributeName.c_str(), attributeName.c_str(), MFnNumericData::kDouble, weight );
        fnNumericAttribute.setMin( 0.0 );
        fnNumericAttribute.setMax( 1.0 );
        status = addAttribute( inWeight[i] );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    {
        MFnNumericAttribute fnNumericAttribute;
        inStartColor =
            fnNumericAttribute.create( "inStartColor", "startColor", MFnNumericData::k3Double, ( 1.0, 0.3, 0.1 ) );
        fnNumericAttribute.setUsedAsColor( true );
        fnNumericAttribute.setDefault( 1.0, 0.3, 0.1 );
        status = addAttribute( inStartColor );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    {
        MFnNumericAttribute fnNumericAttribute;
        inEndColor = fnNumericAttribute.create( "inEndColor", "endColor", MFnNumericData::k3Double, ( 0.1, 0.3, 1.0 ) );
        fnNumericAttribute.setUsedAsColor( true );
        fnNumericAttribute.setDefault( 0.1, 0.3, 1.0 );
        status = addAttribute( inEndColor );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    // inViewportDisplayMode
    // this is mandatory however currently the user is unable to change it therefore all extra fields could be removed
    {
        MFnEnumAttribute fnEnumAttribute;
        inViewportDisplayMode = fnEnumAttribute.create( "inViewportDisplayMode", "displayMode", DISPLAYMODE_DOT1 );
        fnEnumAttribute.addField( "Display As Small Dots", DISPLAYMODE_DOT1 );
        fnEnumAttribute.addField( "Display As Large Dots", DISPLAYMODE_DOT2 );
        status = addAttribute( inViewportDisplayMode );
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

    // render particle count is the only attribute that doesn't affect the viewport, so handle it here.
    attributeAffects( inRenderParticleCount, outParticleStream );

    // This is only a code-reduction strategy: because there are so many dependency attributes, most of
    // which fall into common categories, setting them up as lists helps cut down on what would be
    // otherwise an enourmous code wall of 'attributeAffects' calls
    // It will also increase ease of adding or removing attributes later on
    static MObject s_commonDependencyAttributes[] = { inStartColor, inEndColor, inViewportLoadMode,
                                                      inViewportParticleCount, inViewportParticleCount };

    // set up dependencies between all common attributes
    for( size_t i = 0; i < sizeof( s_commonDependencyAttributes ) / sizeof( MObject ); ++i ) {
        register_viewport_dependency( viewportInputDependencies, s_commonDependencyAttributes[i], outSentinel );

        attributeAffects( s_commonDependencyAttributes[i], outParticleStream );
    }

    for( size_t i = 0; i < MAXTRANSFORMATIONS; ++i ) {
        register_viewport_dependency( viewportInputDependencies, inPosX[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inPosY[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inPosZ[i], outSentinel );

        register_viewport_dependency( viewportInputDependencies, inRotX[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inRotY[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inRotZ[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inRotW[i], outSentinel );

        register_viewport_dependency( viewportInputDependencies, inScaleX[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inScaleY[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inScaleZ[i], outSentinel );

        register_viewport_dependency( viewportInputDependencies, inSkewX[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inSkewY[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inSkewZ[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inSkewW[i], outSentinel );
        register_viewport_dependency( viewportInputDependencies, inSkewA[i], outSentinel );

        register_viewport_dependency( viewportInputDependencies, inWeight[i], outSentinel );

        attributeAffects( inPosX[i], outParticleStream );
        attributeAffects( inPosY[i], outParticleStream );
        attributeAffects( inPosZ[i], outParticleStream );
        attributeAffects( inRotX[i], outParticleStream );
        attributeAffects( inRotY[i], outParticleStream );
        attributeAffects( inRotZ[i], outParticleStream );
        attributeAffects( inRotW[i], outParticleStream );
        attributeAffects( inScaleX[i], outParticleStream );
        attributeAffects( inScaleY[i], outParticleStream );
        attributeAffects( inScaleZ[i], outParticleStream );
        attributeAffects( inSkewX[i], outParticleStream );
        attributeAffects( inSkewY[i], outParticleStream );
        attributeAffects( inSkewZ[i], outParticleStream );
        attributeAffects( inSkewW[i], outParticleStream );
        attributeAffects( inSkewA[i], outParticleStream );
        attributeAffects( inWeight[i], outParticleStream );
    }

    return MS::kSuccess;
}

void PRTFractal::postConstructor() {
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
        FF_LOG( debug ) << "PRTFractal::postConstructor(): setValue for outParticleStream" << std::endl;
        plug.setValue( pluginMpxData );
    }
}

MStatus PRTFractal::getCurrentTime( MDataBlock& block, MTime& outTime ) const {
    MStatus status;
    MDataHandle hValue = block.inputValue( inTime, &status );

    if( status == MS::kSuccess )
        outTime = hValue.asTime();

    return ( status );
}

MBoundingBox PRTFractal::boundingBox() const { return m_boundingBox; }

/**
 * Recalculates the bounding box for this node based on the cached set of particles
 */
void PRTFractal::cacheBoundingBox() {
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

bool PRTFractal::isBounded() const { return true; }

particle_array* PRTFractal::getCachedViewportParticles() {
    touchSentinelOutput();

    return &m_cachedParticles;
}

PRTObject::display_mode_t PRTFractal::getViewportDisplayMode() {
    return (PRTObject::display_mode_t)getIntAttribute( inViewportDisplayMode );
}

MStatus PRTFractal::setDependentsDirty( const MPlug& plug, MPlugArray& plugArray ) {
    // NOTE: This function does not appear to ever be called in OS X when viewport 2.0 is enabled. Thus we have the
    // update hack in PRTObject.
    MObject thisObj = thisMObject();
    check_dependents_dirty( viewportInputDependencies, plug, thisObj );
    return MS::kSuccess;
}

MStatus PRTFractal::compute( const MPlug& plug, MDataBlock& block ) {

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
            MStatus status;
            // get input time
            const MTime currentTime = maya_util::get_current_time();
            // re-compute the cached particle array
            cacheParticlesAt( currentTime );
            cacheBoundingBox();
            status = MS::kSuccess;
            block.setClean( plug );
        }
    } catch( std::exception& e ) {
        MGlobal::displayError( e.what() );
        return MS::kFailure;
    }

    return status;
}

const frantic::geometry::trimesh3& PRTFractal::getRootMesh() const { return get_prt_fractals_icon_mesh(); }

// caches the particles for viewport display purposes
void PRTFractal::cacheParticlesAt( MTime time ) {
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
void PRTFractal::touchSentinelOutput() const {
    MStatus status;
    int value = 0;

    // trying the grab the value from the sentinel plug triggers an update if
    // any of the input plugs are dirty.
    MPlug SentinelPlug( thisMObject(), outSentinel );
    SentinelPlug.getValue( value );
}

particle_istream_ptr PRTFractal::getViewportParticleStream( const frantic::graphics::transform4f& tm,
                                                            const MDGContext& currentContext ) const {
    particle_istream_ptr myStream = getParticleStream( currentContext, krakatoa::enabled_mode::viewport );
    return myStream;
}

particle_istream_ptr PRTFractal::getRenderParticleStream( const frantic::graphics::transform4f& tm,
                                                          const MDGContext& currentContext ) const {
    particle_istream_ptr myStream = getParticleStream( currentContext, krakatoa::enabled_mode::render );
    return myStream;
}

/**
 * Primary method for building particle streams from the input parameters.
 *
 * @param atTime the scene time at which to build the particle stream
 * @param mode whether to build the viewport particle set, or the render particle set
 * @return the combined particle stream object
 */
particle_istream_ptr PRTFractal::getParticleStream( const MDGContext& currentContext,
                                                    krakatoa::enabled_mode::enabled_mode_enum mode ) const {
    krakatoasr::fractal_parameters params;

    for( int i = 0; i < getIntAttribute( inAffineTransformationCount ); i++ ) {
        params.append_affine_transform(
            (float)getDoubleAttribute( inPosX[i] ), (float)getDoubleAttribute( inPosY[i] ),
            (float)getDoubleAttribute( inPosZ[i] ), (float)getDoubleAttribute( inRotX[i] ),
            (float)getDoubleAttribute( inRotY[i] ), (float)getDoubleAttribute( inRotZ[i] ),
            (float)getDoubleAttribute( inRotW[i] ), (float)getDoubleAttribute( inScaleX[i] ),
            (float)getDoubleAttribute( inScaleY[i] ), (float)getDoubleAttribute( inScaleZ[i] ),
            (float)getDoubleAttribute( inSkewX[i] ), (float)getDoubleAttribute( inSkewY[i] ),
            (float)getDoubleAttribute( inSkewZ[i] ), (float)getDoubleAttribute( inSkewW[i] ),
            (float)getDoubleAttribute( inSkewA[i] ), (float)getDoubleAttribute( inWeight[i] ) );
    }

    vector3f colors = getVector3fAttribute( inStartColor );

    params.append_color_gradient( colors[0], colors[1], colors[2], 0 );
    colors = getVector3fAttribute( inEndColor );
    params.append_color_gradient( colors[0], colors[1], colors[2], 1 );
    krakatoasr::particle_stream results;
    if( mode == krakatoa::enabled_mode::viewport ) {
        results =
            krakatoasr::particle_stream::create_from_fractals( getIntAttribute( inViewportParticleCount ), params );
    } else {
        results = krakatoasr::particle_stream::create_from_fractals( getIntAttribute( inRenderParticleCount ), params );
    }
    particle_istream_ptr myStream = results.get_data()->stream;

    return myStream;
}

int PRTFractal::getIntAttribute( MObject attribute, const MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asInt( const_cast<MDGContext&>( context ), outStatus );
}

vector3f PRTFractal::getVector3fAttribute( MObject attribute, const MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    MFnNumericData num( plug.asMObject() );
    double data0;
    double data1;
    double data2;
    num.getData( data0, data1, data2 );
    return vector3f( (float)data0, (float)data1, (float)data2 );
}

double PRTFractal::getDoubleAttribute( MObject attribute, const MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asDouble( const_cast<MDGContext&>( context ), outStatus );
}

bool PRTFractal::getBooleanAttribute( MObject attribute, const MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asBool( const_cast<MDGContext&>( context ), outStatus );
}

MTime PRTFractal::getTimeAttribute( MObject attribute, const MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asMTime( const_cast<MDGContext&>( context ), outStatus );
}

channel_map PRTFractal::get_viewport_channel_map( boost::shared_ptr<particle_istream> inputStream ) const {
    channel_map cm;
    const channel_map& inputCm = inputStream->get_native_channel_map();
    if( inputCm.has_channel( _T("Position") ) )
        cm.define_channel( _T("Position"), 3, frantic::channels::data_type_float32 );
    if( inputCm.has_channel( _T("Color") ) )
        cm.define_channel( _T("Color"), 3, frantic::channels::data_type_float16 );
    cm.end_channel_definition();

    return cm;
}

krakatoa::viewport::RENDER_LOAD_PARTICLE_MODE PRTFractal::getViewportLoadMode() const {
    return (krakatoa::viewport::RENDER_LOAD_PARTICLE_MODE)getIntAttribute( inViewportLoadMode );
}

const MString RandomizeFractals::commandName = "RandomizeFractals";
MString RandomizeFractals::s_pluginPath;

void* RandomizeFractals::creator() { return new RandomizeFractals; }

void RandomizeFractals::setPluginPath( const MString& path ) { s_pluginPath = path; }

const MString RandomizeFractals::getPluginPath() { return s_pluginPath; }

MStatus RandomizeFractals::doIt( const MArgList& args ) {
    int affineTransformationCount = 3;
    int randomSeed = 4545;
    MString node = MString( "" );
    unsigned index;

    index = args.flagIndex( "c", "count" );
    if( MArgList::kInvalidArgIndex != index ) {
        args.get( index + 1, affineTransformationCount );
    }

    index = args.flagIndex( "s", "seed" );
    if( MArgList::kInvalidArgIndex != index ) {
        args.get( index + 1, randomSeed );
    }

    index = args.flagIndex( "n", "node" );
    if( MArgList::kInvalidArgIndex != index ) {
        args.get( index + 1, node );
    }

    if( node != "" ) {
        krakatoasr::fractal_parameters params;
        params.set_from_random( affineTransformationCount, 1, randomSeed );
        for( int i = 0; i < affineTransformationCount; i++ ) {
            vector3f positions = params.get_data()->position[i];
            std::stringstream cmd;
            float data = positions.x;
            cmd << "setAttr \"" << node << ".inPosX" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = positions.y;
            cmd << "setAttr \"" << node << ".inPosY" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = positions.z;
            cmd << "setAttr \"" << node << ".inPosZ" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            frantic::graphics::quat4f rotation = params.get_data()->rotation[i];
            cmd.str( "" );
            data = rotation.x;
            cmd << "setAttr \"" << node << ".inRotX" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = rotation.y;
            cmd << "setAttr \"" << node << ".inRotY" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = rotation.z;
            cmd << "setAttr \"" << node << ".inRotZ" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = rotation.w;
            cmd << "setAttr \"" << node << ".inRotW" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            vector3f scale = params.get_data()->scale[i];

            cmd.str( "" );
            data = scale.x;
            cmd << "setAttr \"" << node << ".inScaleX" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = scale.y;
            cmd << "setAttr \"" << node << ".inScaleY" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = scale.z;
            cmd << "setAttr \"" << node << ".inScaleZ" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            frantic::graphics::quat4f skew = params.get_data()->skewOrientation[i];

            cmd.str( "" );
            data = skew.x;
            cmd << "setAttr \"" << node << ".inSkewX" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = skew.y;
            cmd << "setAttr \"" << node << ".inSkewY" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = skew.z;
            cmd << "setAttr \"" << node << ".inSkewZ" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = skew.w;
            cmd << "setAttr \"" << node << ".inSkewW" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = params.get_data()->skewAngle[i];
            cmd << "setAttr \"" << node << ".inSkewA" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            data = params.get_data()->weight[i];
            cmd << "setAttr \"" << node << ".inWeight" << i << "\" " << data;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
        }
        int max = MAXTRANSFORMATIONS;
        for( int i = affineTransformationCount; i < max; i++ ) {
            std::stringstream cmd;
            cmd << "setAttr \"" << node << ".inPosX" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inPosY" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inPosZ" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inRotX" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inRotY" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inRotZ" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inRotW" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inScaleX" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inScaleY" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inScaleZ" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inSkewX" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inSkewY" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inSkewZ" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inSkewW" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inSkewA" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
            cmd.str( "" );
            cmd << "setAttr \"" << node << ".inWeight" << i << "\" " << 0;
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
        }
    }

    return MStatus::kSuccess;
}

const MString AddFractalTransformKeyframes::commandName = "AddFractalTransformKeyframes";
MString AddFractalTransformKeyframes::s_pluginPath;

void* AddFractalTransformKeyframes::creator() { return new AddFractalTransformKeyframes; }

void AddFractalTransformKeyframes::setPluginPath( const MString& path ) { s_pluginPath = path; }

const MString AddFractalTransformKeyframes::getPluginPath() { return s_pluginPath; }

MStatus AddFractalTransformKeyframes::doIt( const MArgList& args ) {
    MString node = MString( "" );
    int max = MAXTRANSFORMATIONS;
    unsigned index;

    index = args.flagIndex( "n", "node" );

    if( MArgList::kInvalidArgIndex != index ) {
        args.get( index + 1, node );
    }

    index = args.flagIndex( "c", "count" );

    if( MArgList::kInvalidArgIndex != index ) {
        args.get( index + 1, max );
    }

    if( node != "" ) {
        for( int i = 0; i < max; i++ ) {
            std::stringstream cmd;
            cmd << "setKeyframe { \"" << node << ".inPosX" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inPosY" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inPosZ" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inRotX" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inRotY" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inRotZ" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inRotW" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inScaleX" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inScaleY" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inScaleZ" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inSkewX" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inSkewY" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inSkewZ" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inSkewW" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inSkewA" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );

            cmd.str( "" );
            cmd << "setKeyframe { \"" << node << ".inWeight" << i << "\" };";
            MGlobal::executeCommand( MString( cmd.str().c_str() ) );
        }
    }

    return MStatus::kSuccess;
}
