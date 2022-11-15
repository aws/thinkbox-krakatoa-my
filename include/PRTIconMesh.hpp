// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <frantic/geometry/trimesh3.hpp>
#include <frantic/graphics/color3f.hpp>

void build_icon_mesh( const float* vertices, size_t numVertices, const int* faces, size_t numFaces,
                      frantic::geometry::trimesh3& outMesh );

const frantic::geometry::trimesh3& get_default_icon_mesh();

void gl_draw_trimesh( const frantic::geometry::trimesh3& mesh );
