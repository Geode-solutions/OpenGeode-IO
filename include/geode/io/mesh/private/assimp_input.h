/*
 * Copyright (c) 2019 - 2020 Geode-solutions
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
            AssimpMeshInput( absl::string_view filename ) : file_( filename )
            {
                OPENGEODE_EXCEPTION( std::ifstream( file_.data() ).good(),
                    "[AssimpMeshInput] Error while opening file: ", file_ );
            }

            virtual ~AssimpMeshInput() = default;

            bool read_file();

            virtual void build_mesh() = 0;

        protected:
            const aiMesh* assimp_mesh()
            {
                return assimp_mesh_;
            }

            geode::NNSearch3D::ColocatedInfo build_duplicated_vertices(
                geode::SurfaceMesh3D& surface )
            {
                const auto duplicated_vertices =
                    load_duplicated_vertices( assimp_mesh() );
                return build_unique_vertices( duplicated_vertices, surface );
            }

        private:
            std::vector< geode::Point3D > load_duplicated_vertices(
                const aiMesh* paiMesh )
            {
                std::vector< geode::Point3D > duplicated_vertices;
                duplicated_vertices.resize( paiMesh->mNumVertices );
                for( const auto v : geode::Range{ paiMesh->mNumVertices } )
                {
                    duplicated_vertices[v].set_value(
                        0, paiMesh->mVertices[v].x );
                    duplicated_vertices[v].set_value(
                        1, paiMesh->mVertices[v].y );
                    duplicated_vertices[v].set_value(
                        2, paiMesh->mVertices[v].z );
                }
                return duplicated_vertices;
            }

            geode::NNSearch3D::ColocatedInfo build_unique_vertices(
                const std::vector< geode::Point3D >& duplicated_vertices,
                geode::SurfaceMesh3D& surface )
            {
                const geode::NNSearch3D colocater{ duplicated_vertices };
                const auto& vertex_mapping =
                    colocater.colocated_index_mapping( geode::global_epsilon );

                auto builder = geode::SurfaceMeshBuilder3D::create( surface );
                builder->create_vertices( vertex_mapping.nb_unique_points() );
                for( const auto v :
                    geode::Range{ vertex_mapping.nb_unique_points() } )
                {
                    builder->set_point( v, vertex_mapping.unique_points[v] );
                }

                return vertex_mapping;
            }

        private:
            absl::string_view file_;
            Assimp::Importer importer_;
            aiMesh* assimp_mesh_{ nullptr };
        };
    } // namespace detail
} // namespace geode
