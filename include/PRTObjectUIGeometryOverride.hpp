// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MTypes.h>

// for viewport 2.0 only.
#if MAYA_API_VERSION >= 201400

#include <maya/MTypes.h>

#include <maya/MFnDagNode.h>
#include <maya/MHWGeometry.h>
#include <maya/MObject.h>
#include <maya/MPxGeometryOverride.h>
#include <maya/MShaderManager.h>

#include "PRTFractal.hpp"
#include "PRTLoader.hpp"
#include "PRTObject.hpp"

#include <boost/shared_ptr.hpp>

class PRTObjectUIGeometryOverride : public MHWRender::MPxGeometryOverride {
  public:
    static MHWRender::MPxGeometryOverride* create( const MObject& obj );

    PRTObjectUIGeometryOverride( const MObject& obj );
    ~PRTObjectUIGeometryOverride( void ){};

    void updateDG();

    void updateRenderItems( const MDagPath& path, MHWRender::MRenderItemList& list );

#if MAYA_API_VERSION >= 201300
    void populateGeometry( const MHWRender::MGeometryRequirements& requirements,
                           const MHWRender::MRenderItemList& renderItems, MHWRender::MGeometry& data );
#else
    void populateGeometry( const MHWRender::MGeometryRequirements& requirements,
                           MHWRender::MRenderItemList& renderItems, MHWRender::MGeometry& data );
#endif

#if MAYA_API_VERSION >= 201600
    MHWRender::DrawAPI supportedDrawAPIs() const;
#endif

    void cleanUp();

  private:
    MObject m_obj;
    PRTObject* m_cachedObj;
    PRTObject::display_mode_t m_cachedDisplayMode;
    frantic::particles::particle_array* m_cachedParticleArray;
    frantic::channels::channel_map m_cachedChannelMap;
    MHWRender::MVertexBuffer* m_vertexBuffer;
    MHWRender::MVertexBuffer* m_colorBuffer;
    float m_cachedFPS;
    // The following are needed because they are part of many if statement conditions
    bool m_lineBasedDisplayMode, m_pointBasedDisplayMode;
    bool m_validDisplayMode;
    bool m_isParticleEnabledInViewport;
    bool m_positiveChannelStructureSize;
    MColor m_cachedColor;

    void cache_wireframe_color( const MFnDagNode& dagNodeFunctionSet );

    /**
     * Sets up a render item of a specified name
     */
    void setup_render_item( const MString& renderItemName, const MHWRender::MGeometry::Primitive geometryType,
                            MHWRender::MRenderItemList& renderItemList,
                            const MHWRender::MShaderManager& shaderManager );

    /**
     * Creates a new render item object
     */
    MHWRender::MRenderItem* create_render_item( const MString& renderItemName,
                                                const MHWRender::MGeometry::Primitive geometryType,
                                                MHWRender::MRenderItemList& renderItemList );

    /**
     * Attaches a shader to the render item
     */
    void attach_shader( const MHWRender::MShaderManager& shaderManager, MHWRender::MRenderItem& renderItem );

    /**
     * Creates a solid color shader item for vertex render items
     */
    MHWRender::MShaderInstance* create_vertex_item_shader( const MHWRender::MShaderManager& shaderManager );

    /**
     * Sets up the shaders for coloring the render items
     */
    void set_shader_color( MHWRender::MShaderInstance& shader );

    /**
     * Sets the color of the shader of the particles
     */
    void populate_particle_color_buffer();

    /* Creates the vertex buffer using the requirements
     *
     * Buffer is organized as follows:
     * m vertices - particle positions
     * n vertices - icon mesh vertices
     * 8 vertices - icon mesh bounding box vertices
     */
    void create_vertex_buffer( const MHWRender::MVertexBufferDescriptorList& vertexRequirements,
                               MHWRender::MGeometry& data );

    /**
     * Populates the vertex buffer with the positions of the icon mesh's bounding square
     */
    void populate_vertex_buffer( const frantic::geometry::trimesh3& iconMesh, size_t numIconVertices );

    /**
     * Create aquire vertex buffer. Implemented as a function to reduce code necessary for backwards compatability
     */
    float* acquire_vertex_buffer( MHWRender::MVertexBuffer* buffer, const size_t size, const bool write );

    /**
     * Populates the particle positions vertex buffer with the positions of the particles in m_cachedParticleArray
     */
    void populate_particle_positions( float* bufferPositions );

    /**
     * Get a vector channel accessor fromt the channel map.
     */
    frantic::channels::channel_cvt_accessor<frantic::graphics::vector3f> get_vector_channel_accessor();

    /**
     * Populates the particle colors vertex buffer with the colors of the particles in m_cachedParticleArray
     */
    void populate_particle_colors( size_t numIconVertices );

    /**
     * Populates a single particle's color in the vertex buffer for display in a line based display mode.
     */
    void populate_particle_line_color( float* colorBuffer, const int offset, const frantic::graphics::color3f& color );

    /**
     * Populates a single particle's color in the vertex buffer for display in a point based display mode
     */
    void populate_particle_color( float* colorBuffer, const int offset, const frantic::graphics::color3f& color );

    /**
     * Populates the bounding square of the vertexBuffer
     */
    void populate_icon_bounding_square_vertices( const frantic::geometry::trimesh3& iconMesh, float* bufferPositions );

    /**
     * Populates the middle of the vertexBuffer with the verices of the icon mesh
     */
    void populate_icon_mesh_vertices( const frantic::geometry::trimesh3& iconMesh, float* bufferPositions );

    /**
     * Creates and populates an index buffer for the bounding quare of the icon mesh
     */
    void populate_icon_mesh_bounding_square_indices( MHWRender::MGeometry& geometryData,
                                                     const MHWRender::MRenderItem& renderItem,
                                                     const unsigned int vertexOffset );

    /**
     * Creates and populates an index buffer for the icon mesh
     */
    void populate_icon_mesh_indices( const frantic::geometry::trimesh3& iconMesh, MHWRender::MGeometry& geometryData,
                                     const MHWRender::MRenderItem& renderItem, const unsigned int vertexOffset );

    /**
     * Create and populates an index buffer for the particle array
     */
    void populate_viewport_particle_indices( MHWRender::MGeometry& geometryData,
                                             const MHWRender::MRenderItem& renderItem );
};

// TOTAL HACK FOR BECAUSE FOR VIEWPORT 2.0 UPDATING ON OS X
class ForceViewport20Update : public MPxCommand {
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

#endif
