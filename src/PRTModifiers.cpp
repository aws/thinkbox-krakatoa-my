// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "PRTModifiers.hpp"
#include "maya_progress_bar_interface.hpp"

#include <frantic/channels/channel_map.hpp>
#include <frantic/graphics/color3f.hpp>
#include <frantic/maya/MPxParticleStream.hpp>
#include <frantic/maya/attributes.hpp>
#include <frantic/maya/particles/texture_evaluation_particle_istream.hpp>
#include <frantic/maya/util.hpp>
#include <frantic/particles/streams/empty_particle_istream.hpp>

#include <krakatoasr_particles.hpp>
#include <krakatoasr_renderer/params.hpp>

#include <maya/MFnEnumAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MPlugArray.h>

using namespace frantic::graphics;
using namespace frantic::channels;

namespace {

// Available Modifier Types
enum modifier_choices_t {
    NoModifier = 0,
    SetChannelVec3 = 1,
    SetChannel = 2,
    ScaleChannel = 3,
    CopyChannel = 4,
    RepopulateParticles = 5,
    ApplyTexture = 6
};

inline frantic::maya::PRTObjectBase::particle_istream_ptr getEmptyStream() {
    frantic::channels::channel_map lsChannelMap;
    lsChannelMap.define_channel( _T("Position"), 3, frantic::channels::data_type_float32 );
    lsChannelMap.end_channel_definition();

    return frantic::particles::streams::particle_istream_ptr(
        new frantic::particles::streams::empty_particle_istream( lsChannelMap ) );
}

/**
 * Gets an attribute as a string and if invalid treats it as an enum and tries to get its name.
 * This is a temporary function, and only used for the trasition of the last 2.2 build into 2.3.
 * The change we made was to use drop-down enum lists, instead of string channel names.
 * It can be removed and replaced by get_enum_attribute after some time.
 */
inline MString get_string_or_enum_attribute( const MFnDependencyNode& node, const MString& attribute,
                                             const MDGContext& context = MDGContext::fsNormal,
                                             MStatus* outStatus = NULL ) {
    MString result;
    MStatus status;

    result = frantic::maya::get_string_attribute( node, attribute, context, &status );
    if( status != MS::kSuccess ) {
        result = frantic::maya::get_enum_attribute( node, attribute, context, outStatus );
        return result;
    }

    if( outStatus != NULL )
        *outStatus = status;
    return result;
}

} // namespace

const MTypeId PRTModifiers::id( 0x0011748d );
const MString PRTModifiers::typeName = "PRTModifier";

MObject PRTModifiers::inParticleStream;
MObject PRTModifiers::outParticleStream;
MObject PRTModifiers::inEnabled;

MObject PRTModifiers::inModifiersMethod;
MObject PRTModifiers::inModifiersName;

PRTModifiers::PRTModifiers() {}

PRTModifiers::~PRTModifiers() {}

void* PRTModifiers::creator() { return new PRTModifiers; }

MStatus PRTModifiers::initialize() {
    MStatus status;
    MObject defaultString;

    // Particle Stream Input
    {
        MFnTypedAttribute fnTypedAttribute;
        inParticleStream = fnTypedAttribute.create( "inParticleStream", "inParticleStream",
                                                    frantic::maya::MPxParticleStream::id, MObject::kNullObj );
        fnTypedAttribute.setHidden( true );
        fnTypedAttribute.setStorable( false );
        status = addAttribute( inParticleStream );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

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

    // inEnabled
    {
        MFnNumericAttribute fnNumericAttribute;
        inEnabled = fnNumericAttribute.create( "inEnabled", "enabled", MFnNumericData::kBoolean, 1 );
        status = addAttribute( inEnabled );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    // inModifiersMethod
    {
        MFnTypedAttribute fnTypedAttribute;
        inModifiersMethod = fnTypedAttribute.create( "inModifiersMethod", "method", MFnData::kString, defaultString );
        status = addAttribute( inModifiersMethod );
        fnTypedAttribute.setHidden( true );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    // inModifiersName
    {
        MFnTypedAttribute fnTypedAttribute;
        inModifiersName = fnTypedAttribute.create( "inModifiersName", "Name", MFnData::kString, defaultString );
        status = addAttribute( inModifiersName );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }

    attributeAffects( inParticleStream, outParticleStream );
    attributeAffects( inEnabled, outParticleStream );
    attributeAffects( inModifiersMethod, outParticleStream );

    return MS::kSuccess;
}

void PRTModifiers::postConstructor() {
    MStatus stat;

    // With the default node deletion behaviour, modifier nodes are deleted whenever the last node with an attribute
    // connected to an input attribute is created, or when the last node with an attribute connected to an output
    // attribute is deleted. This behaviour is desireable for input attributes, since we want modifiers to be deleted
    // when the PRTObject they modify is deleted, however, we don't want them to be deleted when another node ( such as
    // a Frost ) connected to an output attribute is deleted. The two following calls ensure that we get the desired
    // behaviour. Note that these calls are only respected if made in postConstructor().
    setExistWithoutInConnections( false );
    setExistWithoutOutConnections( true );

    // Output Particles
    {
        // prepare the default data
        MFnPluginData fnData;
        MObject pluginMpxData = fnData.create( MTypeId( frantic::maya::MPxParticleStream::id ), &stat );
        if( stat != MStatus::kSuccess )
            throw std::runtime_error( ( "DEBUG: PRTModifiers: fnData.create()" + stat.errorString() ).asChar() );
        frantic::maya::MPxParticleStream* mpxData =
            frantic::maya::mpx_cast<frantic::maya::MPxParticleStream*>( fnData.data( &stat ) );
        if( !mpxData || stat != MStatus::kSuccess )
            throw std::runtime_error(
                ( "DEBUG: PRTModifiers: dynamic_cast<frantic::maya::MPxParticleStream*>(...) " + stat.errorString() )
                    .asChar() );
        mpxData->setParticleSource( this );

        // get plug
        MObject obj = thisMObject();
        MFnDependencyNode depNode( obj, &stat );
        if( stat != MStatus::kSuccess )
            throw std::runtime_error(
                ( "DEBUG: PRTModifiers: could not get dependencyNode from thisMObject():" + stat.errorString() )
                    .asChar() );

        MPlug plug = depNode.findPlug( "outParticleStream", &stat );
        if( stat != MStatus::kSuccess )
            throw std::runtime_error(
                ( "DEBUG: PRTModifiers: could not find plug 'outParticleStream' from depNode: " + stat.errorString() )
                    .asChar() );

        // set the default data on the plug
        FF_LOG( debug ) << "maya_magma_mpxnode::postConstructor(): setValue for outParticleStream" << std::endl;
        plug.setValue( pluginMpxData );
    }
}

MStatus PRTModifiers::compute( const MPlug& plug, MDataBlock& block ) {
    MStatus status = MPxNode::compute( plug, block );
    try {
        if( plug == outParticleStream ) {
            MDataHandle outputData = block.outputValue( outParticleStream );

            MFnPluginData fnData;
            MObject pluginMpxData = fnData.create( MTypeId( frantic::maya::MPxParticleStream::id ), &status );
            if( status != MStatus::kSuccess )
                return status;
            frantic::maya::MPxParticleStream* mpxData =
                frantic::maya::mpx_cast<frantic::maya::MPxParticleStream*>( fnData.data( &status ) );
            if( mpxData == NULL )
                return MS::kFailure;
            mpxData->setParticleSource( this );

            outputData.set( mpxData );
            status = MS::kSuccess;
        }
    } catch( std::exception& e ) {
        MGlobal::displayError( e.what() );
        return MS::kFailure;
    }

    return status;
}

frantic::maya::PRTObjectBase::particle_istream_ptr
PRTModifiers::getParticleStream( const frantic::graphics::transform4f& objectSpace, const MDGContext& context,
                                 bool isViewport ) const {
    MStatus stat;

    // Get the input particle stream
    MObject obj = thisMObject();
    MFnDependencyNode depNode( obj, &stat );
    if( stat != MStatus::kSuccess ) {
        FF_LOG( debug ) << ( ( "DEBUG: PRTModifiers: could not get dependencyNode from thisMObject():" +
                               stat.errorString() )
                                 .asChar() )
                        << std::endl;
        return getEmptyStream();
    }

    MPlug plug = depNode.findPlug( "inParticleStream", &stat );
    if( stat != MStatus::kSuccess ) {
        FF_LOG( debug ) << ( ( "DEBUG: PRTModifiers: could not find plug 'inParticleStream' from depNode: " +
                               stat.errorString() )
                                 .asChar() )
                        << std::endl;
        return getEmptyStream();
    }

    MObject prtMpxData;
    plug.getValue( prtMpxData );
    MFnPluginData fnData( prtMpxData );
    frantic::maya::MPxParticleStream* streamMPxData =
        frantic::maya::mpx_cast<frantic::maya::MPxParticleStream*>( fnData.data( &stat ) );
    if( streamMPxData == NULL ) {
        FF_LOG( debug ) << ( ( "DEBUG: PRTModifiers: could not find plug 'inParticleStream' from depNode: " +
                               stat.errorString() )
                                 .asChar() )
                        << std::endl;
        return getEmptyStream();
    }

    particle_istream_ptr outStream;
    if( isViewport )
        outStream = streamMPxData->getViewportParticleStream( objectSpace, context );
    else
        outStream = streamMPxData->getRenderParticleStream( objectSpace, context );

    // Get the modifiers transform
    if( isEnabled( context ) ) {
        outStream = apply_channel_modifiers( context, isViewport, outStream );
    }

    return outStream;
}

bool PRTModifiers::isEnabled( const MDGContext& context ) const {
    MStatus outStatus;
    MPlug plug( thisMObject(), inEnabled );
    return plug.asBool( const_cast<MDGContext&>( context ), &outStatus );
}

int PRTModifiers::getModifierMethod( MString& outName, const MDGContext& context ) const {
    MStatus outStatus;
    MPlug plug( thisMObject(), inModifiersMethod );

    outName = plug.asString( const_cast<MDGContext&>( context ), &outStatus );
    if( outStatus != MS::kSuccess )
        return -1;

    // Sanitize string
    MString typeName = outName.toLowerCase();
    MStringArray substrings;

    // Replace spaces with underscore
    typeName.split( ' ', substrings );
    typeName = "";
    for( unsigned int i = 0; i < substrings.length(); i++ ) {
        if( i > 0 )
            typeName += "_";
        typeName += substrings[i];
    }

    int result;
    if( typeName == "" || typeName == "none" ) {
        result = NoModifier;
    } else if( typeName == "set_vector_channel" || typeName == "set_float[3]_channel" ||
               typeName == "set_channel_vec3" ) {
        result = SetChannelVec3;
    } else if( typeName == "set_float_channel" || typeName == "set_channel_float" || typeName == "set_channel" ) {
        result = SetChannel;
    } else if( typeName == "scale_channel" ) {
        result = ScaleChannel;
    } else if( typeName == "copy_channel" ) {
        result = CopyChannel;
    } else if( typeName == "repopulate_particles" ) {
        result = RepopulateParticles;
    } else if( typeName == "apply_texture" ) {
        result = ApplyTexture;
    } else {
        result = -1;
    }
    return result;
}

frantic::particles::streams::particle_istream_ptr
PRTModifiers::apply_channel_modifiers( const MDGContext& currentContext, bool inViewport,
                                       frantic::particles::streams::particle_istream_ptr inputParticles ) const {
    boost::shared_ptr<maya_progress_bar_interface> pMayaProgressBar;

    // we still want to have progress bar shown if viewport enabled
    if( inViewport ) {
        pMayaProgressBar = boost::make_shared<maya_progress_bar_interface>();
        pMayaProgressBar->set_num_frames( 1 );
        pMayaProgressBar->set_current_frame( 0 );

        // We are checking if maya wants to cancel immediately if it does we are resetting the display
        // Maya should only want to cancel immediately if a cancel request was sent after we end the display
        pMayaProgressBar->begin_display();
        if( pMayaProgressBar->is_cancelled() ) {
            pMayaProgressBar->end_display();
            pMayaProgressBar->begin_display();
        }
    }

    krakatoasr::particle_stream particleStream;
    particleStream.get_data()->stream = inputParticles;
    MFnDependencyNode fnNode( thisMObject() );

    MStatus outStatus;
    MString attrName;
    MString modPrefix = "KMod_";

    MString methodName;
    int method = getModifierMethod( methodName, currentContext );
    switch( method ) {
    case ApplyTexture: {
        FF_LOG( debug ) << "Adding Apply Texture modifier." << std::endl;
        attrName = modPrefix + "Evaluate_In_Viewport";
        bool kModEvalInViewport = frantic::maya::get_boolean_attribute( fnNode, attrName, currentContext, &outStatus );
        if( !inViewport || kModEvalInViewport ) {
            attrName = modPrefix + "Channel_Name";
            MString kModChannelName = get_string_or_enum_attribute( fnNode, attrName, currentContext );
            attrName = modPrefix + "Texture";
            MPlug colorPlug = fnNode.findPlug( attrName );
            if( !colorPlug.isConnected() ) {
                color3f kModColor = frantic::maya::get_color_attribute( fnNode, attrName, currentContext );
                krakatoasr::channelop_set_vector( particleStream, kModChannelName.asChar(), kModColor.r, kModColor.g,
                                                  kModColor.b );
            } else {
                MPlugArray colorPlugConnections;
                colorPlug.connectedTo( colorPlugConnections, true, false );
                if( colorPlugConnections.length() >= 1 ) {
                    attrName = modPrefix + "UVW_Channel_Name";
                    MString kModUVWChannelName = get_string_or_enum_attribute( fnNode, attrName, currentContext );
                    MFnDependencyNode connectionNode( colorPlugConnections[0].node() );
                    particleStream.get_data()->stream.reset(
                        new frantic::maya::particles::texture_evaluation_particle_istream(
                            particleStream.get_data()->stream, connectionNode.name().asChar(),
#if defined( FRANTIC_USE_WCHAR )
                            kModUVWChannelName.asWChar(), kModChannelName.asWChar() ) );
#else
                            kModUVWChannelName.asChar(), kModChannelName.asChar() ) );
#endif
                }
            }
        }
    } break;
    case SetChannelVec3: {
        FF_LOG( debug ) << "Adding Set Float3 Channel modifier." << std::endl;
        attrName = modPrefix + "Channel_Name";
        MString kModChannelName = get_string_or_enum_attribute( fnNode, attrName, currentContext );
        attrName = modPrefix + "Channel_Value";
        color3f kModColor = frantic::maya::get_color_attribute( fnNode, attrName, currentContext );
        krakatoasr::channelop_set_vector( particleStream, kModChannelName.asChar(), kModColor.r, kModColor.g,
                                          kModColor.b );
    } break;
    case SetChannel: {
        FF_LOG( debug ) << "Adding Set Float Channel modifier." << std::endl;
        attrName = modPrefix + "Channel_Name";
        MString kModOutput = get_string_or_enum_attribute( fnNode, attrName, currentContext );
        attrName = modPrefix + "Channel_Value";
        float kModValue = frantic::maya::get_float_attribute( fnNode, attrName, currentContext );
        krakatoasr::channelop_set_float( particleStream, kModOutput.asChar(), kModValue );
    } break;
    case ScaleChannel: {
        FF_LOG( debug ) << "Adding Scale Channel modifier." << std::endl;
        attrName = modPrefix + "Channel_Name";
        MString kModOutput = get_string_or_enum_attribute( fnNode, attrName, currentContext );
        attrName = modPrefix + "Channel_Scale";
        float kModScale = frantic::maya::get_float_attribute( fnNode, attrName, currentContext );
        krakatoasr::channelop_scale( particleStream, kModOutput.asChar(), kModScale );
    } break;
    case CopyChannel: {
        FF_LOG( debug ) << "Adding Copy Channel modifier." << std::endl;
        attrName = modPrefix + "Source_Channel";
        MString kModSourceChannel = get_string_or_enum_attribute( fnNode, attrName, currentContext );
        attrName = modPrefix + "Destination_Channel";
        MString kModDestChannel = get_string_or_enum_attribute( fnNode, attrName, currentContext );
        attrName = modPrefix + "Copy_as_Vector_Length";
        bool kModCopyAsVectorLength = frantic::maya::get_boolean_attribute( fnNode, attrName, currentContext );
        if( !kModCopyAsVectorLength ) {
            krakatoasr::channelop_copy( particleStream, kModDestChannel.asChar(), kModSourceChannel.asChar() );
        } else {
            size_t arity =
                ( kModDestChannel == "Density" ) ? 1 : 3; // hard-coding it to 3, except in "Density" case, and cases
                                                          // where the destination channel already exists.
            const channel_map& cm = particleStream.get_data()->stream->get_native_channel_map();
            if( cm.has_channel( frantic::strings::to_tstring( kModDestChannel.asChar() ) ) ) {
                frantic::channels::data_type_t dataTypeUnused;
                cm.get_channel_definition( frantic::strings::to_tstring( kModDestChannel.asChar() ), dataTypeUnused,
                                           arity ); // get the arity of the existing channel.
            }
            krakatoasr::create_vector_magnitude_channel( particleStream, kModDestChannel.asChar(), (int)arity,
                                                         kModSourceChannel.asChar() );
        }
    } break;
    case RepopulateParticles: {
        FF_LOG( debug ) << "Adding Repopulate Particles modifier." << std::endl;
        attrName = modPrefix + "Evaluate_In_Viewport";
        bool kModEvalInViewport = frantic::maya::get_boolean_attribute( fnNode, attrName, currentContext, &outStatus );
        if( !inViewport || kModEvalInViewport ) {
            attrName = modPrefix + "Fill_Radius";
            float kModFillRadius = frantic::maya::get_float_attribute( fnNode, attrName, currentContext );
            attrName = modPrefix + "Fill_Radius_Subdivs";
            int kModFillRadiusSubdivs = frantic::maya::get_int_attribute( fnNode, attrName, currentContext );
            attrName = modPrefix + "Particles_Per_Subdiv";
            int kModNumParticlesPerSubdiv = frantic::maya::get_int_attribute( fnNode, attrName, currentContext );
            attrName = modPrefix + "Density_Falloff";
            float kModFalloff = frantic::maya::get_float_attribute( fnNode, attrName, currentContext );
            attrName = modPrefix + "Random_Seed";
            unsigned kRandomSeed = frantic::maya::get_int_attribute( fnNode, attrName, currentContext );
            krakatoasr::add_particle_repopulation( particleStream, kModFillRadius, kModFillRadiusSubdivs,
                                                   kModNumParticlesPerSubdiv, kModFalloff, kRandomSeed,
                                                   pMayaProgressBar.get(), pMayaProgressBar.get() );
        }
    } break;
    case NoModifier:
        break;
    default:
        throw std::runtime_error( ( "Unknown modifier type: \"" + methodName + "\"" ).asChar() );
    }

    if( inViewport )
        pMayaProgressBar->end_display();

    return particleStream.get_data()->stream;
}
