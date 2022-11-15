// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "KrakatoaParticleJitter.hpp"
#include <stdlib.h>

#include <frantic/maya/maya_util.hpp>
#include <frantic/maya/particles/particles.hpp>

#include <maya/MAnimControl.h>
#include <maya/MArgDatabase.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MSelectionList.h>
#include <maya/MSyntax.h>

#include <boost/random.hpp>
#include <frantic/graphics/vector3f.hpp>

using namespace frantic;
using namespace frantic::channels;
using namespace frantic::graphics;
using namespace frantic::particles;
using namespace frantic::particles::streams;

const MString krakatoaParticleJitter::commandName = "KrakatoaParticleJitter";
MString krakatoaParticleJitter::s_pluginPath;

const char* radiusFlag = "-r";
const char* radiusLongFlag = "-radius";
const char* nodeFlag = "-n";
const char* nodeLongFlag = "-node";
const char* randomSeedFlag = "-s";
const char* randomSeedLongFlag = "-seed";
const char* minIdFlag = "-mId";
const char* minIdLongFlag = "-minId";
const char* channelFlag = "-c";
const char* channelLongFlag = "-channel";

void* krakatoaParticleJitter::creator() { return new krakatoaParticleJitter; }

void krakatoaParticleJitter::setPluginPath( const MString& path ) { s_pluginPath = path; }

const MString krakatoaParticleJitter::getPluginPath() { return s_pluginPath; }

// a streamlined way of setting flags
MSyntax krakatoaParticleJitter::newSyntax() {
    MSyntax syntax;
    syntax.addFlag( randomSeedFlag, randomSeedLongFlag, MSyntax::kLong );
    syntax.addFlag( nodeFlag, nodeLongFlag, MSyntax::kString );
    syntax.addFlag( radiusFlag, radiusLongFlag, MSyntax::kDouble );
    syntax.addFlag( minIdFlag, minIdLongFlag, MSyntax::kLong );
    syntax.addFlag( channelFlag, channelLongFlag, MSyntax::kString );
    return syntax;
}

MStatus krakatoaParticleJitter::doIt( const MArgList& args ) {
    // create and set Default values
    MSelectionList selected;
    MString node;
    int randomSeed = 100;
    double radius = 5;
    int minId = -1;
    MString channel = "position";

    MArgDatabase argData( syntax(), args );
    // get all the data from the arguments fail if we are not given a node
    // we could change node from a flag
    if( argData.isFlagSet( nodeFlag ) ) {
        argData.getFlagArgument( nodeFlag, 0, node );
    } else {
        FF_LOG( warning ) << "Error: No particle node given" << std::endl;
        return MStatus::kFailure;
    }

    if( argData.isFlagSet( randomSeedFlag ) ) {
        argData.getFlagArgument( randomSeedFlag, 0, randomSeed );
    }

    if( argData.isFlagSet( radiusFlag ) ) {
        argData.getFlagArgument( radiusFlag, 0, radius );
    }

    if( argData.isFlagSet( minIdFlag ) ) {
        argData.getFlagArgument( minIdFlag, 0, minId );
    }
    if( argData.isFlagSet( channelFlag ) ) {
        argData.getFlagArgument( channelFlag, 0, channel );
    }
    // add the node by name to a custom selection list without actually selecting it
    selected.add( node );

    if( selected.length() == 0 ) {
        FF_LOG( warning ) << "No nodes found" << std::endl;
        return MStatus::kFailure;
    } else if( selected.length() > 1 ) {
        FF_LOG( warning ) << "Multiple nodes were found, please refine search to only include a single node."
                          << std::endl;
        return MStatus::kFailure;
    }
    int maxid = minId;
    // currently set up so that only a single node can be passed to this point
    // however I am leaving it so that it can be modified to allow multiple later on
    for( unsigned int i = 0; i < selected.length(); ++i ) {
        MStatus stat;
        MObject obj;
        selected.getDependNode( i, obj );
        MFnParticleSystem particlesTest( obj, &stat );
        // try casting the mobject to a particle system
        if( stat != MS::kSuccess || !particlesTest.isValid() ) {
            // let the user know that this particlular node failed
            MFnDependencyNode depNode( obj );
            FF_LOG( error ) << _T( "Node: " ) << frantic::strings::to_tstring( depNode.name().asChar() )
                            << " is not a valid particle node" << std::endl;
            return MStatus::kFailure;
        }

        // Positions/Velocities in the deformed shape appear to be readonly so lets alter the original shape
        MObject sourceObject = obj;
        if( particlesTest.isDeformedParticleShape( &stat ) ) {
            // WARNING: Maya crashes when it tries to read the particleIDs for deformers for some reason...
            sourceObject = particlesTest.originalParticleShape( &stat );
            if( stat != MS::kSuccess || sourceObject == MObject::kNullObj )
                sourceObject = obj;
        }
        MFnParticleSystem particles( sourceObject, &stat );

        MVectorArray attribute;
        // creat random number generator
        boost::mt19937 gen( randomSeed );
        boost::uniform_01<float> range;
        boost::variate_generator<boost::mt19937, boost::uniform_01<float>> rng( gen, range );
        // get the data from the channel we are looking for
        if( channel == "position" ) {
            particles.position( attribute );
        } else if( channel == "velocity" ) {
            particles.velocity( attribute );
        } else if( channel == "acceleration" ) {
            particles.acceleration( attribute );
        } else {
            FF_LOG( warning ) << "The channel specified is not supported.  Currently supported channels are: position, "
                                 "velocity, and acceleration"
                              << std::endl;
            return MStatus::kFailure;
        }

        MIntArray ids;
        particles.particleIds( ids );
        // FF_LOG(error)<<"Particle Count: "<<positions.length()<<std::endl;

        vector3f adjustment;
        for( unsigned int k = 0; k < attribute.length(); k++ ) {
            if( ids[k] > minId ) {
                // get the random adjustment and modify the attribute
                adjustment = frantic::graphics::vector3f::from_random_in_sphere( rng, (float)radius );
                attribute[k].x += adjustment.x;
                attribute[k].y += adjustment.y;
                attribute[k].z += adjustment.z;

                // store the largest id we have found
                if( ids[k] > maxid ) {
                    maxid = ids[k];
                }
            }
        }

        // Maya seems to assume positions are in object space when you set the position.  Need to transform it back.
        if( channel == "position" ) {
            MTime currentTime = MAnimControl::currentTime();
            MDGContext currentContext( currentTime );
            MDagPath particlesPath;
            particles.getPath( particlesPath );
            frantic::graphics::transform4f objectSpace;
            frantic::maya::maya_util::get_object_world_matrix( particlesPath, currentContext, objectSpace );
            objectSpace = objectSpace.to_inverse();
            for( unsigned int k = 0; k < attribute.length(); k++ ) {
                frantic::graphics::vector3f fixedValue( (float)attribute[k].x, (float)attribute[k].y,
                                                        (float)attribute[k].z );
                fixedValue = objectSpace * fixedValue;
                attribute[k].x = fixedValue.x;
                attribute[k].y = fixedValue.y;
                attribute[k].z = fixedValue.z;
            }
        }

        // set the data
        particles.setPerParticleAttribute( channel, attribute, &stat );
        if( stat != MS::kSuccess ) {
            // let the user know we failed on this node
            FF_LOG( error ) << "Failed to set attribute for node "
                            << frantic::strings::to_tstring( particles.name().asChar() ) << std::endl;
            return MStatus::kFailure;
        }
    }
    // return the maximum id we found
    krakatoaParticleJitter::setResult( maxid );
    return MStatus::kSuccess;
}
