/*
 * Copyright (c) 2019 - 2023 Geode-solutions
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#pragma once

#include <fstream>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

#include <geode/basic/logger.h>

#include <geode/geometry/nn_search.h>
#include <geode/geometry/point.h>

#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/core/polygonal_surface.h>

namespace geode
{
    namespace detail
    {
        class AssimpMeshInput
        {
        public:
            AssimpMeshInput(
                absl::string_view filename, SurfaceMesh3D& surface )
                : surface_( surface ),
                  builder_{ SurfaceMeshBuilder3D::create( surface ) },
                  file_( filename )
            {
                OPENGEODE_EXCEPTION( std::ifstream{ to_string( file_ ) }.good(),
                    "[AssimpMeshInput] Error while opening file: ", file_ );
            }

            virtual ~AssimpMeshInput() = default;

            void read_file();

            virtual void build_mesh() = 0;

        protected:
            void build_mesh_from_duplicated_vertices();

            void build_mesh_without_duplicated_vertices();

        private:
            index_t build_vertices( absl::Span< const Point3D > points );

            index_t build_polygons( absl::Span< const index_t > vertex_mapping,
                index_t vertex_offset,
                index_t mesh_id );

            void build_texture( index_t polygon_offset, index_t mesh_id );

        private:
            SurfaceMesh3D& surface_;
            std::unique_ptr< SurfaceMeshBuilder3D > builder_;
            absl::string_view file_;
            Assimp::Importer importer_;
            std::vector< aiMesh* > assimp_meshes_;
            std::vector< std::pair< std::string, std::string > >
                assimp_materials_;
        };
    } // namespace detail
} // namespace geode
