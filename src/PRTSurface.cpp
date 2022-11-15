// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "stdafx.h"

#include "PRTSurface.hpp"
#include "PRTSurfaceIconMesh.hpp"

#include "maya_ksr.hpp"

#include <boost/logic/tribool.hpp>

#include <maya/MFnEnumAttribute.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnPluginData.h>
#include <maya/MFnTypedAttribute.h>

#include <frantic/maya/convert.hpp>
#include <frantic/maya/util.hpp>

#include <frantic/geometry/triangle_utils.hpp>
#include <frantic/particles/streams/empty_particle_istream.hpp>
#include <frantic/particles/streams/fractional_particle_istream.hpp>
#include <frantic/particles/streams/surface_particle_istream.hpp>
#include <frantic/particles/streams/transformed_particle_istream.hpp>

#include <frantic/maya/maya_util.hpp>

using namespace frantic::maya;
using namespace frantic::particles;
using namespace frantic::particles::streams;
using namespace frantic::graphics;
using namespace frantic::geometry;

const MString PRTSurface::drawRegistrantId( "PrtSurfacePlugin" );

// general parameters
MObject PRTSurface::inMesh;
MObject PRTSurface::inMeshTransform;
MObject PRTSurface::inCurrentTransform;
MObject PRTSurface::inUseWorldSpace;
MObject PRTSurface::inUseDensityCompensation;

MObject PRTSurface::outViewportParticleCacheProxy;

// viewport load parameters
MObject PRTSurface::inViewportDisplayMode;
MObject PRTSurface::inEnableInViewport;
MObject PRTSurface::inViewportParticlePercent;
MObject PRTSurface::inEnableViewportParticleLimit;
MObject PRTSurface::inViewportParticleLimit;

// Particle parameters
MObject PRTSurface::inRandomSeed;
MObject PRTSurface::inParticleCount;
MObject PRTSurface::inUseParticleSpacing;
MObject PRTSurface::inParticleSpacing;

// Output Particles
MObject PRTSurface::outParticleStream;

boost::unordered_set<std::string> PRTSurface::viewportInputDependencies;

/**
 * Wrapper around Maya's mesh object for use with surface_particle_istream
 */
class maya_mesh_wrapper {
  private:
    frantic::channels::channel_accessor<frantic::graphics::vector3f> m_posAccessor;
    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> m_velocityAccessor;
    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> m_normalAccessor;
    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> m_textureCoordAccessor;
    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> m_colorAccessor;

    // Data members needed to create Maya Mesh
    // (need to do it this way since Maya's Mesh's copy constructor is private)
    MPxNode m_mesh_source;
    MObject m_inMesh;
    MPlug m_meshPlug;
    MObject m_baseMeshObj;
    MMatrix m_transform;

    // Cache to avoid repeating an expensive API call
    boost::shared_ptr<MColorArray> m_colorCache;
    // Cache the result of has_color() since we'll be calling it a lot
    boost::tribool m_hasColor;

    // Maya Mesh itself
    boost::shared_ptr<MFnMesh> m_workingMesh;

  private:
    // Calculate area of triangle given its 3 vertices
    static inline double area( MPoint p1, MPoint p2, MPoint p3 ) {
        MVector v1( p1 - p2 );
        MVector v2( p1 - p3 );
        double a = ( v1 ^ v2 ).length() / 2;
        return a;
    }

    // Lazily initialize the actual mesh
    boost::shared_ptr<MFnMesh> get_working_mesh() {
        if( m_workingMesh == NULL ) {
            m_meshPlug.getValue( m_baseMeshObj );

            // Cast the object as a mesh (because it is a mesh).
            if( !m_baseMeshObj.hasFn( MFn::kMesh ) )
                throw std::runtime_error(
                    "buildParticleStream error: The provided plug is not of kMesh type. Could not retrieve a mesh." );

            m_workingMesh.reset( new MFnMesh( m_baseMeshObj ) );
        }
        return m_workingMesh;
    }

    bool has_color() {
        if( boost::indeterminate( m_hasColor ) ) {
            boost::shared_ptr<MFnMesh> mesh = get_working_mesh();
            MStatus stat;
            MString colorSetName = mesh->currentColorSetName( &stat );
            if( !stat ) {
                throw std::runtime_error( "maya_mesh_wrapper::has_color Error: unable to get mesh's color set name" );
            }

            if( colorSetName != _T("") && mesh->numColors( colorSetName ) > 0 ) {
                MFnMesh::MColorRepresentation colorRepresentation = mesh->getColorRepresentation( colorSetName, &stat );
                if( !stat ) {
                    throw std::runtime_error(
                        "maya_mesh_wrapper::has_color Error: unable to get color representation from color set: " +
                        std::string( colorSetName.asChar() ) );
                }

                if( colorRepresentation != MFnMesh::kRGB && colorRepresentation != MFnMesh::kRGBA ) {
                    m_hasColor = false;
                } else {
                    m_hasColor = true;
                }
            } else {
                m_hasColor = false;
            }
        }

        return static_cast<bool>( m_hasColor );
    }

  public:
    maya_mesh_wrapper( MPxNode meshSource, MObject inMesh, MMatrix matrix )
        : m_mesh_source( meshSource )
        , m_inMesh( inMesh )
        , m_transform( matrix )
        , m_baseMeshObj()
        , m_meshPlug( meshSource.thisMObject(), m_inMesh )
        , m_colorCache( boost::shared_ptr<MColorArray>() )
        , m_hasColor( boost::indeterminate ) {}

    maya_mesh_wrapper( const maya_mesh_wrapper& other )
        : m_posAccessor( other.m_posAccessor )
        , m_velocityAccessor( other.m_velocityAccessor )
        , m_normalAccessor( other.m_normalAccessor )
        , m_textureCoordAccessor( other.m_textureCoordAccessor )
        , m_mesh_source( other.m_mesh_source )
        , m_inMesh( other.m_inMesh )
        , m_transform( other.m_transform )
        , m_baseMeshObj()
        , m_meshPlug( m_mesh_source.thisMObject(), m_inMesh )
        , m_colorCache( boost::shared_ptr<MColorArray>() )
        , m_hasColor( boost::indeterminate ) {}

    ~maya_mesh_wrapper() {}

    void get_native_map( frantic::channels::channel_map& outNativeMap ) {
        outNativeMap.define_channel<frantic::graphics::vector3f>( _T( "Position" ) );
        // outNativeMap.define_channel<frantic::graphics::vector3f>( _T( "Velocity" ) );
        outNativeMap.define_channel<frantic::graphics::vector3f>( _T( "Normal" ) );
        outNativeMap.define_channel<frantic::graphics::vector3f>( _T( "TextureCoord" ) );

        if( has_color() ) {
            outNativeMap.define_channel<frantic::graphics::vector3f>( _T( "Color" ) );
        }
    }

    void set_channel_map( const frantic::channels::channel_map& seedMap ) {
        m_velocityAccessor.reset();
        m_normalAccessor.reset();
        m_textureCoordAccessor.reset();
        m_posAccessor.reset();
        m_colorAccessor.reset();

        if( seedMap.has_channel( _T( "Position" ) ) )
            m_posAccessor = seedMap.get_accessor<frantic::graphics::vector3f>( _T( "Position" ) );

        // if( seedMap.has_channel( _T( "Velocity" ) ) )
        //	m_velocityAccessor = seedMap.get_cvt_accessor<frantic::graphics::vector3f>( _T( "Velocity" ) );

        if( seedMap.has_channel( _T( "Normal" ) ) )
            m_normalAccessor = seedMap.get_cvt_accessor<frantic::graphics::vector3f>( _T( "Normal" ) );
        if( seedMap.has_channel( _T( "TextureCoord" ) ) )
            m_textureCoordAccessor = seedMap.get_cvt_accessor<frantic::graphics::vector3f>( _T( "TextureCoord" ) );

        if( has_color() && seedMap.has_channel( _T( "Color" ) ) ) {
            m_colorAccessor = seedMap.get_cvt_accessor<frantic::graphics::vector3f>( _T( "Color" ) );
        }
    }

    std::size_t surface_count() {
        boost::shared_ptr<MFnMesh> mesh = get_working_mesh();
        return mesh->numPolygons();
    }

    std::size_t element_count( std::size_t surfaceIndex ) {
        boost::shared_ptr<MFnMesh> mesh = get_working_mesh();

        if( 0 <= surfaceIndex && surfaceIndex < mesh->numPolygons() )
            return mesh->polygonVertexCount( (int)surfaceIndex ) - 2;
        return 0;
    }

    float element_area( std::size_t surfaceIndex, std::size_t elementIndex ) {
        boost::shared_ptr<MFnMesh> mesh = get_working_mesh();

        int triangleVertices[3];
        mesh->getPolygonTriangleVertices( (int)surfaceIndex, (int)elementIndex, triangleVertices );

        MPoint p1, p2, p3;
        mesh->getPoint( triangleVertices[0], p1 );
        mesh->getPoint( triangleVertices[1], p2 );
        mesh->getPoint( triangleVertices[2], p3 );

        p1 = p1 * m_transform;
        p2 = p2 * m_transform;
        p3 = p3 * m_transform;

        return (float)area( p1, p2, p3 );
    }

    template <class RandomGen>
    void seed_particle( char* pOutParticle, std::size_t surfaceIndex, std::size_t elementIndex,
                        RandomGen& randomnessGenerator ) {
        boost::shared_ptr<MFnMesh> mesh = get_working_mesh();

        float bc[3]; // barycentric coordinate

        frantic::geometry::random_barycentric_coordinate( bc, randomnessGenerator );

        int triangleVertices[3];
        mesh->getPolygonTriangleVertices( (int)surfaceIndex, (int)elementIndex, triangleVertices );

        MPoint p1, p2, p3;
        mesh->getPoint( triangleVertices[0], p1 );
        mesh->getPoint( triangleVertices[1], p2 );
        mesh->getPoint( triangleVertices[2], p3 );

        p1 = p1 * m_transform;
        p2 = p2 * m_transform;
        p3 = p3 * m_transform;

        frantic::graphics::vector3f triVerts[] = { frantic::maya::from_maya_t( p1 ), frantic::maya::from_maya_t( p2 ),
                                                   frantic::maya::from_maya_t( p3 ) };

        frantic::graphics::vector3f p = ( bc[0] * triVerts[0] + bc[1] * triVerts[1] + bc[2] * triVerts[2] );

        m_posAccessor.get( pOutParticle ) = p;

        if( m_normalAccessor.is_valid() ) {
            m_normalAccessor.set( pOutParticle,
                                  frantic::graphics::triangle_normal( triVerts[0], triVerts[1], triVerts[2] ) );
        }
        if( m_textureCoordAccessor.is_valid() ) {
            float uCoords[3];
            float vCoords[3];
            // get UV coords for the 3 points in the triangle
            for( int i = 0; i < 3; i++ ) {
                mesh->getPolygonUV( (int)surfaceIndex, i, uCoords[i], vCoords[i], NULL );
            }
            // calculate UV coord for particle
            float uCoord = ( bc[0] * uCoords[0] + bc[1] * uCoords[1] + bc[2] * uCoords[2] );
            float vCoord = ( bc[0] * vCoords[0] + bc[1] * vCoords[1] + bc[2] * vCoords[2] );
            // set particle value
            m_textureCoordAccessor.set( pOutParticle, vector3f( uCoord, vCoord, 0.0f ) );
        }

        if( m_colorAccessor.is_valid() && has_color() ) {
            // generate colors from the polygons.

            MStatus stat;
            MString colorSetName = mesh->currentColorSetName( &stat );
            if( !stat ) {
                throw std::runtime_error(
                    "maya_mesh_wrapper::seed_particle Error: unable to get mesh's color set name" );
            }

            if( !m_colorCache ) {
                MColor defaultColor( 0.0, 0.0, 0.0, 1.0 );
                m_colorCache = boost::make_shared<MColorArray>();
                stat = mesh->getVertexColors( *m_colorCache, &colorSetName, &defaultColor );

                if( !stat ) {
                    throw std::runtime_error(
                        "maya_mesh_wrapper::seed_particle Error: unable to get color index of vertex for color set: " +
                        std::string( colorSetName.asChar() ) );
                }
            }

            frantic::graphics::vector3f colors[3];
            for( int i = 0; i < 3; ++i ) {
                MColor c = ( *m_colorCache )[triangleVertices[i]];
                colors[i] = frantic::graphics::vector3f( c.r, c.g, c.b );
            }

            m_colorAccessor.set( pOutParticle, frantic::graphics::vector3f( bc[0] * colors[0] + bc[1] * colors[1] +
                                                                            bc[2] * colors[2] ) );
        }
    }
};

MStatus PRTSurface::initialize() {
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

    // inRandomSeed
    {
        MFnNumericAttribute fnNumericAttribute;
        inRandomSeed = fnNumericAttribute.create( "inRandomSeed", "randomSeed", MFnNumericData::kInt, 42 );
        status = addAttribute( inRandomSeed );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inUseParticleSpacing
    {
        MFnNumericAttribute fnNumericAttribute;
        inUseParticleSpacing =
            fnNumericAttribute.create( "inUseParticleSpacing", "useParticleSpacing", MFnNumericData::kBoolean, 0 );
        status = addAttribute( inUseParticleSpacing );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inParticleCount
    {
        MFnNumericAttribute fnNumericAttribute;
        inParticleCount = fnNumericAttribute.create( "inParticleCount", "particleCount", MFnNumericData::kInt, 1000 );
        fnNumericAttribute.setMin( 1 );
        fnNumericAttribute.setSoftMax( 10000 );
        status = addAttribute( inParticleCount );
        CHECK_MSTATUS_AND_RETURN_IT( status );
    }
    // inParticleSpacing
    {
        MFnNumericAttribute fnNumericAttribute;
        inParticleSpacing =
            fnNumericAttribute.create( "inParticleSpacing", "particleSpacing", MFnNumericData::kFloat, 1 );
        fnNumericAttribute.setMin( 0.001 );
        fnNumericAttribute.setSoftMin( 0.1 );
        fnNumericAttribute.setSoftMax( 10 );
        status = addAttribute( inParticleSpacing );
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

    // set up the attribute affect relationships between all the attributes (i.e. to force viewport preview updates)
    static MObject s_particleCacheDependencyAttributes[] = { inMesh,
                                                             inRandomSeed,
                                                             inParticleCount,
                                                             inUseParticleSpacing,
                                                             inParticleSpacing,
                                                             inViewportParticlePercent,
                                                             inViewportParticleLimit,
                                                             inViewportParticlePercent,
                                                             inEnableInViewport,
                                                             inEnableViewportParticleLimit,
                                                             inUseWorldSpace,
                                                             inMeshTransform,
                                                             inCurrentTransform };

    for( size_t i = 0; i < sizeof( s_particleCacheDependencyAttributes ) / sizeof( MObject ); ++i ) {
        register_viewport_dependency( viewportInputDependencies, s_particleCacheDependencyAttributes[i],
                                      outViewportParticleCacheProxy );

        attributeAffects( s_particleCacheDependencyAttributes[i], outParticleStream );
    }

    viewportInputDependencies.insert( std::string( MFnAttribute( inViewportDisplayMode ).name().asChar() ) );

    return MStatus::kSuccess;
}

void PRTSurface::postConstructor() {
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
        FF_LOG( debug ) << "PRTSurface::postConstructor(): setValue for outParticleStream" << std::endl;
        plug.setValue( pluginMpxData );
    }
}

void* PRTSurface::creator() { return new PRTSurface; }

MTypeId PRTSurface::typeId = 0x0011748b;
MString PRTSurface::typeName = "PRTSurface";

PRTSurface::PRTSurface() {
    cacheBoundingBox();
    m_osxViewport20HackInitialized = false;
}

PRTSurface::~PRTSurface() {}

// inherited from MPxSurfaceShape
MBoundingBox PRTSurface::boundingBox() const {
    // maya won't actually trigger a recompute of these intermediate values unless you manually touch them beforehand
    if( inWorldSpace() ) {
        touchSentinelOutput( inMeshTransform );
        touchSentinelOutput( inCurrentTransform );
    }

    touchSentinelOutput( outViewportParticleCacheProxy );

    return m_boundingBox;
}

/**
 * Recalculates the bounding box for this node based on the cached set of particles
 */
void PRTSurface::cacheBoundingBox() {
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

frantic::channels::channel_map PRTSurface::get_viewport_channel_map(
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
void PRTSurface::cacheViewportParticles() {
    FF_LOG( debug ) << this->name().asChar() << " Calling Cache Viewport" << std::endl;
    if( !getBooleanAttribute( inEnableInViewport ) ) {
        m_cachedParticles.clear();
        return;
    }

    const MTime currentTime = frantic::maya::maya_util::get_current_time();
    MDGContext currentContext( currentTime );
    frantic::graphics::transform4f tm = getTransform( currentContext );

    MFnDependencyNode fnNode( thisMObject() );
    particle_istream_ptr particleStream = PRTObjectBase::getFinalParticleStream( fnNode, tm, currentContext, true );

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

particle_istream_ptr PRTSurface::buildParticleStream( bool viewportMode ) const {
    channel_map channelMap;
    channelMap.define_channel( _T("Position"), 3, frantic::channels::data_type_float32 );
    channelMap.define_channel( _T("Normal"), 3, frantic::channels::data_type_float16 );
    // channelMap.define_channel( _T("Velocity"), 3, frantic::channels::data_type_float16 );
    channelMap.end_channel_definition();

    MPlug meshPlug( thisMObject(), inMesh );

    if( !meshPlug.isConnected() ) {
        // Not connected to any geometry so there are no particles
        return particle_istream_ptr( new empty_particle_istream( channelMap ) );
    }

    MMatrix transform;
    if( inWorldSpace() ) {
        transform = getMayaWorldSpaceTransform( MDGContext::fsNormal );
    } else {
        transform = MMatrix::identity;
    }
    boost::shared_ptr<frantic::particles::streams::surface_particle_istream<maya_mesh_wrapper>> result(
        new frantic::particles::streams::surface_particle_istream<maya_mesh_wrapper>(
            maya_mesh_wrapper( *this, PRTSurface::inMesh, transform ) ) );

    result->set_channel_map( channelMap );

    int seed = getRandomSeed();
    result->set_random_seed( static_cast<boost::uint32_t>( seed ) );

    bool useSpacing = isUseParticleSpacing();
    if( useSpacing ) {
        double particleSpacing = getParticleSpacing();
        result->set_particle_spacing( (float)particleSpacing );
    } else {
        int particleCount = getParticleCount();
        result->set_particle_count( particleCount );
    }

    particle_istream_ptr mystream = result;
    return mystream;
}

const frantic::geometry::trimesh3& PRTSurface::getRootMesh() const { return get_prt_surface_icon_mesh(); }

bool PRTSurface::isBounded() const {
    // I'm just not going to bother with trying to compute the bounding box correctly in world-space, right now, its
    // just too weird, plus I can't figure out how to trigger a change on this node's worldspace matrix
    return !inWorldSpace();
}

MMatrix PRTSurface::getMayaWorldSpaceTransform( const MDGContext& context, bool* isOK ) const {
    MPlug matrixPlug( thisMObject(), inMeshTransform );

    MObject matrixObject;
    MStatus status = matrixPlug.getValue( matrixObject, const_cast<MDGContext&>( context ) );
    if( status ) {

        MFnMatrixData worldMatrixData( matrixObject, &status );
        if( status ) {

            MMatrix worldMatrix = worldMatrixData.matrix( &status );
            if( status ) {
                if( isOK != NULL )
                    ( *isOK ) = true;
                return worldMatrix;
            }
        }
    }

    if( isOK != NULL )
        ( *isOK ) = false;
    return MMatrix::identity;
}

frantic::graphics::transform4f PRTSurface::getWorldSpaceTransform( const MDGContext& context, bool* isOK ) const {
    MMatrix worldMatrix = getMayaWorldSpaceTransform( context, isOK );
    frantic::graphics::transform4f outTransform = frantic::maya::from_maya_t( worldMatrix );
    return outTransform;
}

MStatus PRTSurface::setDependentsDirty( const MPlug& plug, MPlugArray& plugArray ) {
    // NOTE: This function does not appear to ever be called in OS X when viewport 2.0 is enabled. Thus we have the
    // update hack in PRTObject.
    MObject thisObj = thisMObject();
    check_dependents_dirty( viewportInputDependencies, plug, thisObj );
    return MS::kSuccess;
}

// inherited from MPxNode
MStatus PRTSurface::compute( const MPlug& plug, MDataBlock& block ) {
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

        } else {
            status = MStatus::kUnknownParameter;
        }
    } catch( std::exception& e ) {
        MGlobal::displayError( e.what() );
        return MStatus::kFailure;
    }

    return status;
}

particle_istream_ptr PRTSurface::getRenderParticleStream( const frantic::graphics::transform4f& tm,
                                                          const MDGContext& currentContext ) const {
    particle_istream_ptr particleStream = buildParticleStream( false );
    if( inWorldSpace() ) {
        particleStream = boost::shared_ptr<particle_istream>(
            new transformed_particle_istream<float>( particleStream, tm.to_inverse() ) );
    }
    return particleStream;
}

particle_istream_ptr PRTSurface::getViewportParticleStream( const frantic::graphics::transform4f& tm,
                                                            const MDGContext& currentContext ) const {
    particle_istream_ptr particleStream = buildParticleStream( true );
    particleStream = apply_fractional_particle_istream( particleStream, getViewportParticleFraction(),
                                                        getViewportParticleLimit(), true );
    if( inWorldSpace() ) {
        particleStream = boost::shared_ptr<particle_istream>(
            new transformed_particle_istream<float>( particleStream, tm.to_inverse() ) );
    }
    return particleStream;
}

frantic::particles::particle_array* PRTSurface::getCachedViewportParticles() {
    // maya won't actually trigger a recompute of these intermediate values unless you manually touch them beforehand
    touchSentinelOutput( outViewportParticleCacheProxy );
    return &m_cachedParticles;
}

PRTObject::display_mode_t PRTSurface::getViewportDisplayMode() {
    return (PRTObject::display_mode_t)getIntAttribute( inViewportDisplayMode );
}

bool PRTSurface::inWorldSpace() const { return getBooleanAttribute( inUseWorldSpace ); }

int PRTSurface::getRandomSeed() const { return getIntAttribute( inRandomSeed ); }

int PRTSurface::getParticleCount() const { return getIntAttribute( inParticleCount ); }

double PRTSurface::getParticleSpacing() const { return getDoubleAttribute( inParticleSpacing ); }

bool PRTSurface::isUseParticleSpacing() const { return getBooleanAttribute( inUseParticleSpacing ); }

bool PRTSurface::useDensityCompensation() const { return getBooleanAttribute( inUseDensityCompensation ); }

double PRTSurface::getViewportParticleFraction() const {
    return getDoubleAttribute( inViewportParticlePercent ) / 100.0;
}

boost::int64_t PRTSurface::getViewportParticleLimit() const {
    const bool enableLimit = getBooleanAttribute( inEnableViewportParticleLimit );
    return enableLimit ? ( static_cast<boost::int64_t>( getDoubleAttribute( inViewportParticleLimit ) * 1000.0 ) )
                       : std::numeric_limits<boost::int64_t>::max();
}

int PRTSurface::getIntAttribute( MObject attribute, MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asInt( context, outStatus );
}

double PRTSurface::getDoubleAttribute( MObject attribute, MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asDouble( context, outStatus );
}

bool PRTSurface::getBooleanAttribute( MObject attribute, MDGContext& context, MStatus* outStatus ) const {
    MPlug plug( thisMObject(), attribute );
    return plug.asBool( context, outStatus );
}

void PRTSurface::touchSentinelOutput( MObject sentinelAttribute ) const {
    MStatus status;
    int value = 0;
    // trying the grab the value from the sentinel plug triggers an update if
    // any of the input plugs are dirty.
    MPlug SentinelPlug( thisMObject(), sentinelAttribute );
    SentinelPlug.getValue( value );
}
