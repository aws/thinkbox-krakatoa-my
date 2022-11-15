// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#include "PRTObjectUIGeometryOverride.hpp"
#include <stdafx.h>

// for viewport 2.0 only.
#include <maya/MGlobal.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MSelectionList.h>

static const MString ICON_ITEM_NAME = "icon";
static const MString ICON_BBOX_ITEM_NAME = "iconMeshBoundingBox";
static const MString VERTICES_ITEM_NAME = "vertices";

MHWRender::MPxGeometryOverride* PRTObjectUIGeometryOverride::create( const MObject& obj ) {
    return new PRTObjectUIGeometryOverride( obj );
}

PRTObjectUIGeometryOverride::PRTObjectUIGeometryOverride( const MObject& obj )
    : MHWRender::MPxGeometryOverride( obj )
    , m_obj( obj )
    , m_vertexBuffer( NULL )
    , m_colorBuffer( NULL )
    , m_cachedParticleArray( NULL )
    , m_cachedObj( NULL ) {
    MStatus stat;
    MFnDependencyNode node( obj, &stat );

    if( stat ) {
        m_cachedObj = dynamic_cast<PRTObject*>( node.userNode() );
    }
}

void PRTObjectUIGeometryOverride::updateDG() {
    MStatus status;
    MFnDagNode objFunctionSet;

    status = objFunctionSet.setObject( m_obj );

    if( status ) {
        cache_wireframe_color( objFunctionSet );
    }

    if( m_cachedObj ) {
        m_cachedParticleArray = m_cachedObj->getCachedViewportParticles();

        if( m_cachedDisplayMode != m_cachedObj->getViewportDisplayMode() ) {
            m_cachedDisplayMode = m_cachedObj->getViewportDisplayMode();
        }

        if( m_cachedParticleArray ) {
            m_cachedChannelMap = m_cachedParticleArray->get_channel_map();

            // Cached boolean values to prevent function overhead of repeated function calls
            m_lineBasedDisplayMode = ( m_cachedDisplayMode == PRTObject::DISPLAYMODE_NORMAL &&
                                       m_cachedChannelMap.has_channel( _T( "Normal" ) ) ) ||
                                     ( m_cachedDisplayMode == PRTObject::DISPLAYMODE_VELOCITY &&
                                       m_cachedChannelMap.has_channel( _T( "Velocity" ) ) ) ||
                                     ( m_cachedDisplayMode == PRTObject::DISPLAYMODE_TANGENT &&
                                       m_cachedChannelMap.has_channel( _T( "Tangent" ) ) );
            m_pointBasedDisplayMode = m_cachedDisplayMode == PRTObject::DISPLAYMODE_DOT1 ||
                                      m_cachedDisplayMode == PRTObject::DISPLAYMODE_DOT2;
            m_validDisplayMode = m_lineBasedDisplayMode || m_pointBasedDisplayMode;

            m_positiveChannelStructureSize = ( m_cachedChannelMap.structure_size() > 0 ) ? true : false;

            if( m_positiveChannelStructureSize )
                m_isParticleEnabledInViewport = ( m_cachedParticleArray->particle_count() > 0 );
            else
                m_isParticleEnabledInViewport = false;

            m_cachedFPS = (float)MTime( 1.0, MTime::kSeconds ).as( MTime::uiUnit() );
        }
    }
}

void PRTObjectUIGeometryOverride::cache_wireframe_color( const MFnDagNode& dagNodeFunctionSet ) {
    MStatus status;
    MDagPath dagPath;

    status = dagNodeFunctionSet.getPath( dagPath );

    if( status ) {
        m_cachedColor = MHWRender::MGeometryUtilities::wireframeColor( dagPath );
    }
}

void PRTObjectUIGeometryOverride::updateRenderItems( const MDagPath& path, MHWRender::MRenderItemList& list ) {
    using namespace MHWRender;
    MRenderer* renderer = MRenderer::theRenderer();

    if( !renderer )
        return;

    const MShaderManager* shaderManager = renderer->getShaderManager();

    if( !shaderManager )
        return;

    setup_render_item( ICON_ITEM_NAME, MGeometry::kTriangles, list, *shaderManager );
    setup_render_item( ICON_BBOX_ITEM_NAME, MGeometry::kLines, list, *shaderManager );

    if( m_lineBasedDisplayMode ) {
        setup_render_item( VERTICES_ITEM_NAME, MGeometry::kLines, list, *shaderManager );
    } else {
        setup_render_item( VERTICES_ITEM_NAME, MGeometry::kPoints, list, *shaderManager );
    }
}

void PRTObjectUIGeometryOverride::setup_render_item( const MString& renderItemName,
                                                     const MHWRender::MGeometry::Primitive geometryType,
                                                     MHWRender::MRenderItemList& renderItemList,
                                                     const MHWRender::MShaderManager& shaderManager ) {

    using namespace MHWRender;
    int index = 0;
    MRenderItem* renderItem = NULL;

    // Must have a special case for vertex items, since depending on the display mode they can either be points or lines
    // and renderItemList doesn't provide functionality for searching for both
    if( renderItemName == VERTICES_ITEM_NAME ) {
        index = renderItemList.indexOf( renderItemName, MGeometry::kLines, MGeometry::kAll );

        // If a vertex render item with primitive type kLines is not found, check for one of type kPoints
        if( index < 0 ) {
            index = renderItemList.indexOf( renderItemName, MGeometry::kPoints, MGeometry::kAll );
        }

    } else {
        index = renderItemList.indexOf( renderItemName, geometryType, MGeometry::kAll );
    }

    if( renderItemName == VERTICES_ITEM_NAME ) {
        // Check to see if the vertex render item exists, if it doesn't create it, if it does get the index of it.
        // ***NOTE*** If the display mode is invalid (ie velocity display mode without a velocity channel), remove the
        // vertex render item completely, so that it doesn't cause problems with the rendering of the icon mesh.
        if( index < 0 && m_validDisplayMode && m_isParticleEnabledInViewport ) {
            renderItem = create_render_item( renderItemName, geometryType, renderItemList );
        } else if( index >= 0 && m_validDisplayMode && m_isParticleEnabledInViewport ) {
            // Render item exists, so get it from the list
            renderItem = renderItemList.itemAt( index );

            // If the vertex render item's primitive type is not the same as the one passed to this function, delete and
            // create a new render item
            if( renderItem && renderItem->primitive() != geometryType ) {
                renderItemList.removeAt( index );
                renderItem = create_render_item( renderItemName, geometryType, renderItemList );
            }
        } else if( index >= 0 ) {
            renderItemList.removeAt( index );
        }
    } else {
        if( index < 0 ) {
            // Create the render item and append it to the list of render items
            renderItem = create_render_item( renderItemName, geometryType, renderItemList );
        } else {
            // Render item exists, so get it from the list
            renderItem = renderItemList.itemAt( index );
        }
    }

    // Enable render items
    if( renderItem ) {
        attach_shader( shaderManager, *renderItem );
        renderItem->enable( true );
    }
}

MHWRender::MRenderItem*
PRTObjectUIGeometryOverride::create_render_item( const MString& renderItemName,
                                                 const MHWRender::MGeometry::Primitive geometryType,
                                                 MHWRender::MRenderItemList& renderItemList ) {
    using namespace MHWRender;
    MRenderItem* newItem = MRenderItem::Create( renderItemName, geometryType, MGeometry::kAll, false );

    if( newItem ) {
        renderItemList.append( newItem );
    }

    return newItem;
}

void PRTObjectUIGeometryOverride::attach_shader( const MHWRender::MShaderManager& shaderManager,
                                                 MHWRender::MRenderItem& renderItem ) {
    using namespace MHWRender;
    const float VERTEX_COLOR[] = { 1.f, 1.f, 1.f, 1.f };
    const float LINE_WIDTH[] = { 1.f, 1.f };
    MShaderInstance* shader = NULL;

    if( renderItem.name() == VERTICES_ITEM_NAME ) {
        // Handles the colors for the PRTFractal particles or PRTLoader
        shader = create_vertex_item_shader( shaderManager );
    } else if( renderItem.name() == ICON_BBOX_ITEM_NAME ) {
        // If there are particles and they have a color channel, get a color per vertex shader( cannot be solid shader
        // because having a CPV shader and a solid shader on two different RenderItems causes a crash )
        if( m_cachedChannelMap.has_channel( _T( "Color" ) ) && m_validDisplayMode )
            shader = shaderManager.getStockShader( MShaderManager::k3dCPVThickLineShader );
        else
            shader = shaderManager.getStockShader( MShaderManager::k3dThickLineShader );

        if( shader ) {
            if( !m_cachedChannelMap.has_channel( _T( "Color" ) ) || !m_validDisplayMode )
                set_shader_color( *shader );

            shader->setParameter( "lineWidth", LINE_WIDTH );
        }
    } else if( renderItem.name() == ICON_ITEM_NAME ) {
        // If there are particles and they have a color channel, get a color per vertex shader( cannot be solid shader
        // because having a CPV shader and a solid shader on two different RenderItems causes a crash )
        if( m_cachedChannelMap.has_channel( _T( "Color" ) ) && m_validDisplayMode )
            shader = shaderManager.getStockShader( MShaderManager::k3dCPVSolidShader );
        else
            shader = shaderManager.getStockShader( MShaderManager::k3dSolidShader );

        if( shader && ( !m_cachedChannelMap.has_channel( _T( "Color" ) ) || !m_validDisplayMode ) ) {
            set_shader_color( *shader );
        }
    }

    // Throw an exception if no shader is retrieved, since a null shader will cause a crash
    if( !shader )
        throw std::runtime_error( "PRTObjectUIGeometryOverride.attach_shader: Failed to attach shader." );

    // Attach shader to render item
    if( shader ) {
        renderItem.setShader( shader );
        shaderManager.releaseShader( shader );
    }
}

MHWRender::MShaderInstance*
PRTObjectUIGeometryOverride::create_vertex_item_shader( const MHWRender::MShaderManager& shaderManager ) {
    using namespace MHWRender;
    const float POINT_SIZE[] = { 4.f, 4.f };
    const float LINE_WIDTH[] = { 1.f, 1.f };
    const float VERTEX_COLOR[] = { 1.f, 1.f, 1.f, 1.f };

    MShaderInstance* shader = NULL;

    // Creates the appropriate shader, if a color channel is defined then a color-per-vertex shader is used, if there is
    // no color channel defined a solid shader is used
    if( m_cachedDisplayMode == PRTObject::DISPLAYMODE_DOT2 ) {
        if( !m_cachedChannelMap.has_channel( _T( "Color" ) ) ) {
            shader = shaderManager.getStockShader( MShaderManager::k3dFatPointShader );
        } else
            shader = shaderManager.getStockShader( MShaderManager::k3dCPVFatPointShader );

        if( shader )
            shader->setParameter( "pointSize", POINT_SIZE );
    } else if( m_lineBasedDisplayMode ) {
        if( !m_cachedChannelMap.has_channel( _T( "Color" ) ) )
            shader = shaderManager.getStockShader( MShaderManager::k3dThickLineShader );
        else
            shader = shaderManager.getStockShader( MShaderManager::k3dCPVThickLineShader );

        if( shader )
            shader->setParameter( "lineWidth", LINE_WIDTH );
    } else {
        if( !m_cachedChannelMap.has_channel( _T( "Color" ) ) )
            shader = shaderManager.getStockShader( MShaderManager::k3dSolidShader );
        else
            shader = shaderManager.getStockShader( MShaderManager::k3dCPVSolidShader );
    }

    // Throw and exception if no shader is selected, because having a null shader will cause a crash later on if this is
    // allowed to continue executing
    if( !shader )
        throw std::runtime_error( "PRTObjectUIGeometryOverride.create_vertex_item_shader: Failed to create shader." );

    // If no color channel is present in the channel map, the shader is a solid shader, so assign it a default color
    if( !m_cachedChannelMap.has_channel( _T( "Color" ) ) )
        shader->setParameter( "solidColor", VERTEX_COLOR );

    return shader;
}

void PRTObjectUIGeometryOverride::set_shader_color( MHWRender::MShaderInstance& shader ) {
    float shaderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    shaderColor[0] = m_cachedColor.r;
    shaderColor[1] = m_cachedColor.g;
    shaderColor[2] = m_cachedColor.b;
    shaderColor[3] = m_cachedColor.a;

    shader.setParameter( "solidColor", shaderColor );
}

void PRTObjectUIGeometryOverride::populateGeometry( const MHWRender::MGeometryRequirements& requirements,
                                                    const MHWRender::MRenderItemList& renderItems,
                                                    MHWRender::MGeometry& data ) {
    using namespace MHWRender;

    if( m_cachedObj ) {
        const frantic::geometry::trimesh3 iconMesh( m_cachedObj->getRootMesh() );
        const int numItems = renderItems.length();

        create_vertex_buffer( requirements.vertexRequirements(), data );
        populate_vertex_buffer( iconMesh, iconMesh.vertex_count() );

        const int numParticles = m_cachedParticleArray->particle_count();
        const int numParticleVerticies = m_lineBasedDisplayMode ? numParticles * 2 : numParticles;

        // For each RenderItem setup their its index buffer
        for( int i = 0; i < numItems; ++i ) {
            // Order of items in list is guaranteed
            const MRenderItem* item = renderItems.itemAt( i );

            if( !item )
                continue;

            if( item->name() == VERTICES_ITEM_NAME ) {
                if( m_validDisplayMode && m_cachedParticleArray->get_channel_map().structure_size() > 0 ) {
                    // Only populate the particle indice buffer if the channel map has the correct channel for the
                    // current display mode
                    populate_viewport_particle_indices( data, *item );
                }
            } else if( item->name() == ICON_BBOX_ITEM_NAME ) {
                // Create an index buffer for the icon meshes' bounding box
                populate_icon_mesh_bounding_square_indices( data, *item, numParticleVerticies );
            } else if( item->name() == ICON_ITEM_NAME ) {
                // Create an index buffer for the icon mesh
                populate_icon_mesh_indices( iconMesh, data, *item, numParticleVerticies + 4 );
            }
        }
    }
}

#if MAYA_API_VERSION >= 201600

MHWRender::DrawAPI PRTObjectUIGeometryOverride::supportedDrawAPIs() const {
    return MHWRender::kOpenGL | MHWRender::kOpenGLCoreProfile | MHWRender::kDirectX11;
}

#endif

/* Creates the vertex buffer using the requirements
 *
 * Buffer is organized as follows:
 * m vertices - particle positions
 * 4 vertices - icon mesh bounding square
 * n vertices - icon mesh vertices
 */
void PRTObjectUIGeometryOverride::create_vertex_buffer(
    const MHWRender::MVertexBufferDescriptorList& vertexRequirements, MHWRender::MGeometry& data ) {
    // Handle the vertex requirements
    for( int j = 0; j < vertexRequirements.length(); ++j ) {
        MHWRender::MVertexBufferDescriptor desc;
        if( vertexRequirements.getDescriptor( j, desc ) ) {
            switch( desc.semantic() ) {
            case MHWRender::MGeometry::kColor:
                // Creates the colour vertex buffer
                m_colorBuffer = data.createVertexBuffer( desc );
                break;
            case MHWRender::MGeometry::kPosition:
                // Create the vertex buffer. The indices of the vertex and their corresponding colour must be the same
                m_vertexBuffer = data.createVertexBuffer( desc );
                break;
            }
        }
    }
}

/* Populates the vertex buffer with the bounding box, icon mesh, and mesh vertices
 *
 * See comment before createVertexBuffer() for a description of how the vertices are laid
 * out in the buffer
 */
void PRTObjectUIGeometryOverride::populate_vertex_buffer( const frantic::geometry::trimesh3& iconMesh,
                                                          size_t numIconVertices ) {
    if( m_vertexBuffer && m_cachedParticleArray ) {
        const int COMPONENTS_PER_VERTICE = 3;
        float* bufferPositions = NULL;
        unsigned int verticesPerParticle = 0;

        // Determine the number of vertices needed for each particle
        if( m_isParticleEnabledInViewport && m_validDisplayMode ) {
            if( m_lineBasedDisplayMode ) {
                verticesPerParticle = 2;
            } else if( m_pointBasedDisplayMode ) {
                verticesPerParticle = 1;
            }
        }

        // Acquire the raw float buffer that the particle's and icon's vertices will be stored in
        if( m_positiveChannelStructureSize ) {
            bufferPositions = acquire_vertex_buffer(
                m_vertexBuffer, numIconVertices + 4 + m_cachedParticleArray->particle_count() * verticesPerParticle,
                true );
        } else {
            bufferPositions = acquire_vertex_buffer( m_vertexBuffer, numIconVertices + 4, true );
        }

        if( bufferPositions ) {
            float* bufferPtr = bufferPositions;

            // Only populate the positions and colours of the particles if the display mode is correct, and the channel
            // map has the corresponding channels to that display mode
            if( m_validDisplayMode && m_isParticleEnabledInViewport ) {
                populate_particle_positions( bufferPtr );
                populate_particle_colors( numIconVertices );
                bufferPtr += COMPONENTS_PER_VERTICE * verticesPerParticle * m_cachedParticleArray->particle_count();
            }

            populate_icon_bounding_square_vertices( iconMesh, bufferPtr );
            bufferPtr += COMPONENTS_PER_VERTICE * 4; // 4 for the bounding sqaure

            populate_icon_mesh_vertices( iconMesh, bufferPtr );

            m_vertexBuffer->commit( bufferPositions );
        }
    }
}

/**
 * Create aquire vertex buffer. Implemented as a function to reduce code necessary for backwards compatability
 */
float* PRTObjectUIGeometryOverride::acquire_vertex_buffer( MHWRender::MVertexBuffer* buffer, const size_t bufferSize,
                                                           const bool write ) {
#if MAYA_API_VERSION >= 201300
    return reinterpret_cast<float*>( buffer->acquire( static_cast<unsigned int>( bufferSize ), true ) );
#else
    return reinterpret_cast<float*>( buffer->acquire( static_cast<unsigned int>( bufferSize ) ) );
#endif
}

void PRTObjectUIGeometryOverride::populate_particle_positions( float* bufferPositions ) {
    // Have to check the structure size before checking the particle count because if structure size == 0 then particle
    // count will cause a integer division by zero
    if( bufferPositions && m_cachedParticleArray && m_cachedChannelMap.has_channel( _T( "Position" ) ) &&
        m_validDisplayMode && m_positiveChannelStructureSize && m_cachedParticleArray->particle_count() > 0 ) {
        frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> nAccessor;
        frantic::channels::channel_accessor<frantic::graphics::vector3f> pAccessor =
            m_cachedChannelMap.get_accessor<frantic::graphics::vector3f>( _T( "Position" ) );
        int i = 0;

        nAccessor = get_vector_channel_accessor();

        for( frantic::particles::particle_array::const_iterator it = m_cachedParticleArray->begin();
             it != m_cachedParticleArray->end(); ++it ) {
            frantic::graphics::vector3f currParticlePosition;
            frantic::graphics::vector3f currParticleVector;
            int componentsPerVertice = 3;

            currParticlePosition = pAccessor.get( *it );

            if( m_lineBasedDisplayMode ) {
                componentsPerVertice = 6;
                currParticleVector = nAccessor.get( *it );
            }

            // Put the particle's position in the vertex buffer
            for( int j = 0; j < 3; ++j )
                bufferPositions[componentsPerVertice * i + j] = currParticlePosition[j];

            if( m_lineBasedDisplayMode ) {
                // If a line based display mode is selected, populate the end point of the line
                if( m_cachedDisplayMode == PRTObject::DISPLAYMODE_VELOCITY ) {
                    for( int j = 0; j < 3; ++j )
                        bufferPositions[componentsPerVertice * i + 3 + j] =
                            currParticlePosition[j] + ( 1 / m_cachedFPS ) * currParticleVector[j];
                } else {
                    for( int j = 0; j < 3; ++j )
                        bufferPositions[componentsPerVertice * i + 3 + j] =
                            currParticlePosition[j] + currParticleVector[j];
                }
            }

            i++;
        }
    }
}

/**
 * Get a vector channel accessor fromt the channel map.
 */
frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f>
PRTObjectUIGeometryOverride::get_vector_channel_accessor() {
    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> nAccessor;

    if( m_cachedDisplayMode == PRTObject::DISPLAYMODE_NORMAL && m_cachedChannelMap.has_channel( _T( "Normal" ) ) ) {
        nAccessor = m_cachedChannelMap.get_cvt_accessor<frantic::graphics::vector3f>( _T( "Normal" ) );
    } else if( m_cachedDisplayMode == PRTObject::DISPLAYMODE_VELOCITY &&
               m_cachedChannelMap.has_channel( _T( "Velocity" ) ) ) {
        nAccessor = m_cachedChannelMap.get_cvt_accessor<frantic::graphics::vector3f>( _T( "Velocity" ) );
    } else if( m_cachedDisplayMode == PRTObject::DISPLAYMODE_TANGENT &&
               m_cachedChannelMap.has_channel( _T( "Tangent" ) ) ) {
        nAccessor = m_cachedChannelMap.get_cvt_accessor<frantic::graphics::vector3f>( _T( "Tangent" ) );
    }

    return nAccessor;
}

void PRTObjectUIGeometryOverride::populate_particle_colors( size_t numIconVertices ) {
    // Have to check the structure size before checking the particle count because if structure size == 0 then particle
    // count will cause a integer division by zero
    if( m_colorBuffer && m_cachedParticleArray && m_positiveChannelStructureSize &&
        m_cachedParticleArray->particle_count() > 0 && m_cachedChannelMap.has_channel( _T( "Color" ) ) ) {
        frantic::channels::channel_cvt_accessor<frantic::graphics::color3f> cAccessor =
            m_cachedChannelMap.get_cvt_accessor<frantic::graphics::color3f>( _T( "Color" ) );
        float* colorBuffer = NULL;

        // Acquire a raw float buffer that the color values will be stored in
        if( m_lineBasedDisplayMode ) {
            colorBuffer = acquire_vertex_buffer(
                m_colorBuffer, numIconVertices + 4 + m_cachedParticleArray->particle_count() * 2, true );
        } else if( m_pointBasedDisplayMode ) {
            colorBuffer = acquire_vertex_buffer( m_colorBuffer,
                                                 numIconVertices + 4 + m_cachedParticleArray->particle_count(), true );
        }

        if( colorBuffer ) {
            int bufferOffset = 0;
            int offsetToIcon = 0;
            int verticesPerParticle = 0;

            for( frantic::particles::particle_array::const_iterator it = m_cachedParticleArray->begin();
                 it != m_cachedParticleArray->end(); ++it ) {
                frantic::graphics::color3f currParticleColor = cAccessor.get( *it );

                if( m_lineBasedDisplayMode ) {
                    populate_particle_line_color( colorBuffer, bufferOffset, currParticleColor );
                } else if( m_pointBasedDisplayMode ) {
                    populate_particle_color( colorBuffer, bufferOffset, currParticleColor );
                }

                bufferOffset++;
            }

            // Records the buffer offset to the beginning of where the icon mesh colors start
            offsetToIcon = bufferOffset;
            bufferOffset = 0;

            // Determines the number of vertices needed per particle
            if( m_lineBasedDisplayMode ) {
                verticesPerParticle = 2;
            } else if( m_pointBasedDisplayMode ) {
                verticesPerParticle = 1;
            }

            // Populates the color buffer for the icon meshes' bounding box
            for( int i = 0; i < 4; ++i ) {
                for( int j = 0; j < 4; ++j ) {
                    colorBuffer[4 * bufferOffset + verticesPerParticle * 4 * offsetToIcon + j] = m_cachedColor[j];
                }
                bufferOffset++;
            }

// There seems to be a compiler bug in clang which causes the variable j to not behave as expected. Without j being
// defined as volatile on OSX, the color buffer is written to at unexpected indices, which causes Maya to crash when we
// try to write to indices outside the range of the buffer.
#if defined( __APPLE__ )
            typedef volatile int os_x_volatile_int_t;
#else
            typedef int os_x_volatile_int_t;
#endif

            // Populates the color buffer for the icon mesh
            for( size_t i = 0; i < numIconVertices; ++i ) {
                for( os_x_volatile_int_t j = 0; j < 4; ++j ) {
                    colorBuffer[4 * bufferOffset + verticesPerParticle * 4 * offsetToIcon + j] = m_cachedColor[j];
                }
                bufferOffset++;
            }

            m_colorBuffer->commit( colorBuffer );
        }
    }
}

/**
 * Populates a single particle's color in the vertex buffer for display in a line based display mode.
 */
void PRTObjectUIGeometryOverride::populate_particle_line_color( float* colorBuffer, const int offset,
                                                                const frantic::graphics::color3f& color ) {
    colorBuffer[8 * offset] = color.r;
    colorBuffer[8 * offset + 4] = color.r;

    colorBuffer[8 * offset + 1] = color.g;
    colorBuffer[8 * offset + 5] = color.g;

    colorBuffer[8 * offset + 2] = color.b;
    colorBuffer[8 * offset + 6] = color.b;

    colorBuffer[8 * offset + 3] = 1.0f;
    colorBuffer[8 * offset + 7] = 1.0f;
}

/**
 * Populates a single particle's color in the vertex buffer for display in a point based display mode
 */
void PRTObjectUIGeometryOverride::populate_particle_color( float* colorBuffer, const int offset,
                                                           const frantic::graphics::color3f& color ) {
    colorBuffer[4 * offset] = color.r;
    colorBuffer[4 * offset + 1] = color.g;
    colorBuffer[4 * offset + 2] = color.b;
    colorBuffer[4 * offset + 3] = 1.0f;
}

/**
 * Populates the bounding square of the vertexBuffer
 */
void PRTObjectUIGeometryOverride::populate_icon_bounding_square_vertices( const frantic::geometry::trimesh3& iconMesh,
                                                                          float* bufferPositions ) {
    const int CORNER_OFFSET =
        2; // An offset to select the corners of the bounding cube, that we want for the bounding box

    if( bufferPositions && m_cachedParticleArray ) {
        frantic::geometry::boundbox3f iconBoundingBox = iconMesh.compute_bound_box();

        for( int i = 0; i < 4; ++i ) {
            for( int j = 0; j < 3; ++j )
                bufferPositions[3 * i + j] = iconBoundingBox.get_corner( i + CORNER_OFFSET )[j];
        }
    }
}

/* Populates the middle of the vertexBuffer with the verices of the icon mesh */
void PRTObjectUIGeometryOverride::populate_icon_mesh_vertices( const frantic::geometry::trimesh3& iconMesh,
                                                               float* bufferPositions ) {
    if( bufferPositions && m_cachedParticleArray ) {
        const std::vector<frantic::graphics::vector3f>& iconMeshVertices = iconMesh.vertices_ref();

        for( int i = 0; i < iconMesh.vertex_count(); ++i ) {
            for( int j = 0; j < 3; ++j )
                bufferPositions[3 * i + j] = iconMeshVertices[i][j];
        }
    }
}

/* Creates and populates an index buffer for the icon mesh */
void PRTObjectUIGeometryOverride::populate_icon_mesh_indices( const frantic::geometry::trimesh3& iconMesh,
                                                              MHWRender::MGeometry& geometryData,
                                                              const MHWRender::MRenderItem& renderItem,
                                                              const unsigned int vertexOffset ) {

    using namespace MHWRender;
    MIndexBuffer* indexBuffer = geometryData.createIndexBuffer( MGeometry::kUnsignedInt32 );

    if( indexBuffer && m_cachedParticleArray ) {
        // Acquire a raw unsigned int buffer to store the index values in
        unsigned int* buffer = reinterpret_cast<unsigned int*>(
            indexBuffer->acquire( static_cast<unsigned int>( 3 * iconMesh.face_count() ), true ) );

        // Populate the index buffer
        if( buffer ) {
            std::vector<frantic::graphics::vector3> meshFaces = iconMesh.faces_ref();

            // Place the face's vertex indices in the buffer
            for( int i = 0; i < iconMesh.face_count(); ++i ) {
                for( int j = 0; j < 3; ++j )
                    buffer[3 * i + j] = meshFaces[i][j] + vertexOffset;
            }

            indexBuffer->commit( buffer );
            renderItem.associateWithIndexBuffer( indexBuffer );
        }
    }
}

void PRTObjectUIGeometryOverride::populate_icon_mesh_bounding_square_indices( MHWRender::MGeometry& geometryData,
                                                                              const MHWRender::MRenderItem& renderItem,
                                                                              const unsigned int vertexOffset ) {
    using namespace MHWRender;
    MIndexBuffer* indexBuffer = geometryData.createIndexBuffer( MGeometry::kUnsignedInt32 );

    if( indexBuffer ) {
        // Acquire a raw unsigned int buffer to place the bounding box's indices in
        unsigned int* buffer = reinterpret_cast<unsigned int*>( indexBuffer->acquire( 8, true ) );

        if( buffer ) {
            buffer[0] = vertexOffset;
            buffer[1] = vertexOffset + 1;

            buffer[2] = vertexOffset + 1;
            buffer[3] = vertexOffset + 3;

            buffer[4] = vertexOffset + 3;
            buffer[5] = vertexOffset + 2;

            buffer[6] = vertexOffset + 2;
            buffer[7] = vertexOffset;

            indexBuffer->commit( buffer );

            renderItem.associateWithIndexBuffer( indexBuffer );
        }
    }
}

void PRTObjectUIGeometryOverride::populate_viewport_particle_indices( MHWRender::MGeometry& geometryData,
                                                                      const MHWRender::MRenderItem& renderItem ) {
    using namespace MHWRender;
    MIndexBuffer* indexBuffer = geometryData.createIndexBuffer( MGeometry::kUnsignedInt32 );

    if( indexBuffer && m_cachedParticleArray && m_positiveChannelStructureSize &&
        m_cachedParticleArray->particle_count() > 0 && m_validDisplayMode ) {
        unsigned int* buffer = NULL;

        // Acquire a raw unsigned int buffer to place the particles' indices in
        if( m_lineBasedDisplayMode ) {
            buffer = reinterpret_cast<unsigned int*>( indexBuffer->acquire(
                static_cast<unsigned int>( m_cachedParticleArray->particle_count() * 2 ), true ) );
        } else if( m_pointBasedDisplayMode ) {
            buffer = reinterpret_cast<unsigned int*>(
                indexBuffer->acquire( static_cast<unsigned int>( m_cachedParticleArray->particle_count() ), true ) );
        }

        if( buffer ) {
            // Populate the index buffer
            for( int i = 0; i < m_cachedParticleArray->particle_count(); ++i ) {
                if( m_lineBasedDisplayMode ) {
                    // If line based display mode, put the indices of both the vertices of the line in the buffer
                    buffer[2 * i] = 2 * i;
                    buffer[2 * i + 1] = 2 * i + 1;
                } else if( m_pointBasedDisplayMode ) {
                    buffer[i] = i;
                }
            }

            indexBuffer->commit( buffer );
            renderItem.associateWithIndexBuffer( indexBuffer );
        }
    }
}

void PRTObjectUIGeometryOverride::cleanUp() {}

//
// TOTAL HACK FOR BECAUSE FOR VIEWPORT 2.0 UPDATING ON OS X
//

const MString ForceViewport20Update::commandName = "ForceViewport20Update";
MString ForceViewport20Update::s_pluginPath;

void* ForceViewport20Update::creator() { return new ForceViewport20Update; }

void ForceViewport20Update::setPluginPath( const MString& path ) { s_pluginPath = path; }

const MString ForceViewport20Update::getPluginPath() { return s_pluginPath; }

MStatus ForceViewport20Update::doIt( const MArgList& args ) {
    // get input parameter--the name of the object.
    MString inObjectName;
    MStatus result = args.get( 0, inObjectName );

    // Get the node by name.
    MSelectionList list;
    MGlobal::getSelectionListByName( inObjectName, list );
    MObject objDepNode;
    list.getDependNode( 0, objDepNode );

    // call draw dirty on it, and viewport 2.0 will request an update.
    MHWRender::MRenderer::setGeometryDrawDirty( objDepNode );
    return MStatus::kSuccess;
}