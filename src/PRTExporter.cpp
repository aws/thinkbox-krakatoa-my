// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include <maya/MArgList.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnParticleSystem.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MPlug.h>
#include <maya/MPxCommand.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MVectorArray.h>

#include <boost/algorithm/string.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <frantic/channels/channel_map.hpp>
#include <frantic/channels/named_channel_data.hpp>
#include <frantic/graphics/transform4f.hpp>
#include <frantic/graphics/vector3f.hpp>
#include <frantic/logging/global_progress_logger.hpp>
#include <frantic/logging/progress_logger.hpp>
#include <frantic/particles/particle_array.hpp>
#include <frantic/particles/particle_file_stream_factory.hpp>
#include <frantic/particles/streams/particle_array_particle_istream.hpp>
#include <frantic/particles/streams/particle_istream.hpp>
#include <frantic/particles/streams/particle_ostream.hpp>
#include <frantic/particles/streams/prt_particle_ostream.hpp>
#include <frantic/particles/streams/transformed_particle_istream.hpp>

#include <utility>

#include "PRTExporter.hpp"
#include "PRTFractal.hpp"
#include "PRTLoader.hpp"
#include "PRTSurface.hpp"
#include "PRTVolume.hpp"
#include "maya_ksr.hpp"
#include <frantic/maya/PRTMayaParticle.hpp>
#include <frantic/maya/maya_util.hpp>
#include <frantic/maya/particles/particles.hpp>
#include <frantic/maya/util.hpp>

using namespace frantic;
using namespace frantic::graphics;
using namespace frantic::channels;
using namespace frantic::logging;
using namespace frantic::particles;
using namespace frantic::particles::streams;
using namespace frantic::maya;

namespace {

typedef std::pair<data_type_t, std::size_t> channel_type;

bool is_vector_channel_type( channel_type type ) {
    return is_channel_data_type_float( type.first ) && type.second == 3;
}

bool is_float_channel_type( channel_type type ) { return is_channel_data_type_float( type.first ) && type.second == 1; }

bool is_int_channel_type( channel_type type ) { return is_channel_data_type_signed( type.first ); }

channel_type parse_channel_type( const char* channelTypeString ) {
    channel_type result = channel_data_type_and_arity_from_string( frantic::strings::to_tstring( channelTypeString ) );

    if( is_vector_channel_type( result ) || is_float_channel_type( result ) || is_int_channel_type( result ) ) {
        return result;
    } else {
        return std::make_pair( data_type_invalid, 0 );
    }
}

/**
 * Select the default set of channels to read into the PRT file if no channels
 * are explicitly selected.
 */
void initialize_default_prt_channels( channel_map& selectedChannels ) {
    selectedChannels.define_channel( frantic::maya::particles::PRTPositionChannelName, 3, data_type_float32 );
    selectedChannels.define_channel( frantic::maya::particles::PRTVelocityChannelName, 3, data_type_float16 );
    selectedChannels.define_channel( frantic::maya::particles::PRTColorChannelName, 3, data_type_float16 );
    selectedChannels.define_channel( frantic::maya::particles::PRTParticleIdChannelName, 1, data_type_int64 );
    selectedChannels.define_channel( frantic::maya::particles::PRTDensityChannelName, 1, data_type_float32 );
}

} // anonymous namespace

const MString prt_exporter::commandName = "PRTExporter";

void* prt_exporter::creator() { return new prt_exporter; }

prt_exporter::prt_exporter() {}

prt_exporter::~prt_exporter() {}

MStatus prt_exporter::doIt( const MArgList& args ) {
    try {
        // error check the input
        if( args.length() < 2 ) {
            MGlobal::displayError(
                "PRT Exporter takes three string arguments. Argument one is the path to the .prt output file. Agument "
                "two is the name of the Particle or NParticle source object. (optional) Argument 3 is an array of the "
                "form { \"channelName\", \"channelType\", ... }, where names and types of each channel must be "
                "specified as interleaved pairs." );
            return MStatus::kFailure;
        }

        // get the parameters
        MStatus status;
        unsigned int val = 1;
        MString filename( args.asString( 0, &status ) );
        MStringArray inputObjects;
        inputObjects = args.asStringArray( val, &status );
        if( status != MStatus::kSuccess ) {
            MGlobal::displayError( "Second input must be a string or string array" );
            return MStatus::kFailure;
        }
        // holds which channels the caller requested be exported
        // export_channel_info_map selectedChannels;
        channel_map selectedChannels;
        frantic::particles::particle_file_stream_factory_object streamFactory;

        // Check if specific channels were requested in the PRT export
        if( args.length() > 2 ) {
            // the argument location for an array must be passed by reference
            unsigned int arrayLocation = 2;
            MStringArray dataChannels( args.asStringArray( arrayLocation ) );

            if( dataChannels.length() % 2 != 0 ) {
                MGlobal::displayError(
                    "Channel name and type information must form interleaved pairs in the input array" );
                return MStatus::kFailure;
            }

            // cycle through each interleaved pair of channel name and type, and enter it into the table
            for( unsigned int i = 0; i < dataChannels.length(); i += 2 ) {
                MString channelNameInput = dataChannels[i];
                MString channelTypeInput = dataChannels[i + 1];
                frantic::tstring mayaChannelName = frantic::strings::to_tstring( channelNameInput.asChar() );
                frantic::tstring prtChannelName;
                frantic::maya::particles::get_prt_channel_name_default( mayaChannelName, prtChannelName );

                // error if the same channel is specified twice (or is this an error?)
                if( selectedChannels.has_channel( prtChannelName ) ) {
                    std::ostringstream errorText;
                    errorText << "Channel \'" << mayaChannelName.c_str() << "\' was selected more than once";
                    MGlobal::displayError( errorText.str().c_str() );
                    return MStatus::kFailure;
                }

                channel_type type = parse_channel_type( channelTypeInput.asChar() );

                if( type.first == data_type_invalid ) {
                    std::ostringstream errorText;
                    errorText << "Invalid channel data type : \"" << channelTypeInput.asChar()
                              << "\", must be in the form <type><bits>[arity]";
                    MGlobal::displayError( errorText.str().c_str() );
                    return MStatus::kFailure;
                }

                selectedChannels.define_channel( prtChannelName, type.second, type.first );
            }
        }

        // if no channel selections were made, use a default set of channels to output
        if( selectedChannels.channel_count() == 0 ) {
            initialize_default_prt_channels( selectedChannels );
        }

        selectedChannels.end_channel_definition();
        // get the object from the name by iterating through the scene graph
        MDagPath exportObjectPath;
        MObject exportObject;

        particle_file_stream_factory_object factoryObject;
        factoryObject.set_coordinate_system( frantic::maya::get_coordinate_system() );
        factoryObject.set_length_unit_in_meters( frantic::maya::get_scale_to_meters() );
        factoryObject.set_frame_rate( (unsigned)frantic::maya::get_fps(), 1 );
        boost::shared_ptr<frantic::particles::streams::particle_ostream> outputStream = factoryObject.create_ostream(
            frantic::strings::to_tstring( filename.asChar() ), selectedChannels, selectedChannels );

        MSelectionList list;
        MStatus stat;
        for( unsigned int i = 0; i < inputObjects.length(); i++ ) {
            // create a custom Selection list independent of the active selection list
            // merging the new file names with those already included in the list
            stat = list.add( inputObjects[i], true );
            if( stat != MStatus::kSuccess ) {
                MGlobal::displayError( MString( "The object \"" ) + inputObjects[i] + "\" was not found." );
                return MStatus::kFailure;
            }
        }
        const MTime currentTime = maya_util::get_current_time();
        size_t pSize = outputStream->get_channel_map().structure_size();
        std::vector<char> defaultParticle( pSize, 0 );
        if( outputStream->get_channel_map().has_channel( _T( "Density" ) ) )
            outputStream->get_channel_map().get_cvt_accessor<float>( _T( "Density" ) ).set( &defaultParticle[0], 1.0f );
        if( outputStream->get_channel_map().has_channel( _T( "Color" ) ) )
            outputStream->get_channel_map()
                .get_cvt_accessor<vector3f>( _T( "Color" ) )
                .set( &defaultParticle[0], vector3f( 1.0f ) );
        for( unsigned int i = 0; i < list.length(); i++ ) {
            list.getDagPath( i, exportObjectPath );
            list.getDependNode( i, exportObject );

            MObject exportObjectSource;
            if( exportObject.apiType() == MFn::kParticle || exportObject.apiType() == MFn::kNParticle ) {
                MStatus status;
                MFnParticleSystem mayaParticleStream( exportObject );
                exportObjectSource =
                    frantic::maya::PRTMayaParticle::getPRTMayaParticleFromMayaParticleStreamCheckDeformed(
                        mayaParticleStream, &status );

                // May Particles no longer supported
                if( status != MS::kSuccess || exportObjectSource == MObject::kNullObj ) {
                    MGlobal::displayError( MString( "Maya Particles no longer supported for \"" ) + inputObjects[i] +
                                           "\".  Please wrap with PRTMayaParticle" );
                    return MStatus::kFailure;
                }
            } else {
                exportObjectSource = exportObject;
            }

            MFnDependencyNode fnNode( exportObjectSource );
            if( PRTObjectBase::hasParticleStreamMPxData( fnNode ) ) {
                MObject endOfChain = PRTObjectBase::getEndOfStreamChain( fnNode );
                MFnDependencyNode depNode( endOfChain );

                frantic::graphics::transform4f transform;
                bool ok = frantic::maya::maya_util::get_object_world_matrix( exportObjectPath, currentTime, transform );

                boost::shared_ptr<particle_istream> inputStream =
                    PRTObjectBase::getParticleStreamFromMPxData( depNode, transform, currentTime, false );
                inputStream = maya_ksr::get_renderer_stream_modifications(
                    inputStream, currentTime ); // calls apply_common_operations_to_stream

                // transform the the particle stream into world-space co-ordinates
                transform4f objectPosition;
                maya_util::get_object_world_matrix( exportObjectPath, currentTime, objectPosition );
                inputStream = boost::shared_ptr<particle_istream>(
                    new transformed_particle_istream<float>( inputStream, objectPosition ) );

                inputStream->set_channel_map( outputStream->get_channel_map() );
                inputStream->set_default_particle( &defaultParticle[0] );

                std::vector<char> particleBuffer( inputStream->get_channel_map().structure_size() );
                while( inputStream->get_particle( &particleBuffer[0] ) )
                    outputStream->put_particle( &particleBuffer[0] );
            } else {
                // can't process this object
                MGlobal::displayError( MString( "The object \"" ) + fnNode.name() +
                                       "\" was not a Particle, NParticle, or Thinkbox PRT object.  No output particle "
                                       "stream attribute found." );
                return MStatus::kFailure;
            }
        }
        outputStream->close();
    } catch( const std::exception& e ) {
        MGlobal::displayError( e.what() );
        return MStatus::kFailure;
    }

    return MStatus::kSuccess;
}
