// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "GenerateStickyChannelsCommand.hpp"

#include <krakatoa/birth_channel_gen.hpp>

const MString GenerateStickyChannels::commandName = "GenerateStickyChannels";

void* GenerateStickyChannels::creator() { return new GenerateStickyChannels; }

GenerateStickyChannels::GenerateStickyChannels() {}

GenerateStickyChannels::~GenerateStickyChannels() {}

MStatus GenerateStickyChannels::doIt( const MArgList& args ) {
    try {
        // error check the input
        const int numExpectedArgs = 9;
        if( args.length() < numExpectedArgs ) {
            MGlobal::displayError(
                "Birth Channel Generator takes nine arguments. "
                "Argument one is the path to the input .prt file. "
                "Argument two is the path to the output .prt file. "
                "Argument 3 is the input channel. "
                "Argument 4 is the output channel. "
                "Argument 5 is the ID channel. "
                "Argument 6 is the startframe. "
                "Argument 7 is IgnoreError (bool). Set it as true to ignore per-frame errors and continue to next "
                "frame. "
                "Argument 8 is OverwriteChannel (bool). If true then existing birth channels will always be "
                "overwritten. If false then existing birth channels will never be overwritten. "
                "Argument 9 is OverwriteFile (bool). If true then any exsting PRT files will always be overwritten. If "
                "false then existing files will never be overwritten." );
            return MStatus::kFailure;
        }

        // get the parameters
        MStatus status;
        birth_channel_gen::options opts;

        MString inSequence( args.asString( 0, &status ) );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "First input must be a string" );
            return MStatus::kFailure;
        }
        MString outSequence( args.asString( 1, &status ) );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "Second input must be a string" );
            return MStatus::kFailure;
        }
        MString inChannel( args.asString( 2, &status ) );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "Third input must be a string" );
            return MStatus::kFailure;
        }
        MString outChannel( args.asString( 3, &status ) );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "Fourth input must be a string" );
            return MStatus::kFailure;
        }
        MString idChannel( args.asString( 4, &status ) );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "Fifth input must be a string" );
            return MStatus::kFailure;
        }
        double startFrame = args.asDouble( 5, &status );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "Sixth input must be a double" );
            return MStatus::kFailure;
        }
        bool ignoreError = args.asBool( 6, &status );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "Seventh input must be a bool" );
            return MStatus::kFailure;
        }
        bool overwriteChannel = args.asBool( 7, &status );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "Eighth input must be a bool" );
            return MStatus::kFailure;
        }
        bool overwriteFile = args.asBool( 8, &status );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "Ninth input must be a bool" );
            return MStatus::kFailure;
        }

        opts.m_inSequence = frantic::strings::to_tstring( inSequence.asChar() );
        opts.m_outSequence = frantic::strings::to_tstring( outSequence.asChar() );
        opts.m_inChannel = frantic::strings::to_tstring( inChannel.asChar() );
        opts.m_outChannel = frantic::strings::to_tstring( outChannel.asChar() );
        opts.m_idChannel = frantic::strings::to_tstring( idChannel.asChar() );
        opts.m_startFrame = startFrame;
        opts.m_ignoreError = ignoreError;
        if( overwriteChannel ) {
            opts.m_overwriteChannel = birth_channel_gen::scope_answer::yes_all;
        } else {
            opts.m_overwriteChannel = birth_channel_gen::scope_answer::no_all;
        }
        if( overwriteFile ) {
            opts.m_overwriteFile = birth_channel_gen::scope_answer::yes_all;
        } else {
            opts.m_overwriteFile = birth_channel_gen::scope_answer::no_all;
        }

        birth_channel_gen::generate_sticky_channels( opts );
    } catch( const std::exception& e ) {
        MGlobal::displayError( e.what() );
        return MStatus::kFailure;
    }

    return MStatus();
}
