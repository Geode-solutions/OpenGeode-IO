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
            AssimpMeshInput( absl::string_view filename ) : file_( filename )
            {
                OPENGEODE_EXCEPTION( std::ifstream{ to_string( file_ ) }.good(),
                    "[AssimpMeshInput] Error while opening file: ", file_ );
            }

            virtual ~AssimpMeshInput() = default;

            void read_file();

            virtual void build_mesh() = 0;

        protected:
            index_t nb_meshes() const
            {
                return assimp_meshes_.size();
            }

            const aiMesh* assimp_mesh( index_t mesh )
            {
                return assimp_meshes_[mesh];
            }

            void build_mesh_from_duplicated_vertices( SurfaceMesh3D& surface )
            {
                for( const auto m : geode::Range{ nb_meshes() } )
                {
                    const auto output = build_duplicated_vertices( surface, m );
                    build_polygons( surface, std::get< 0 >( output ),
                        std::get< 1 >( output ), m );
                }
                auto builder = geode::SurfaceMeshBuilder3D::create( surface );
                builder->compute_polygon_adjacencies();
            }

            std::tuple< NNSearch3D::ColocatedInfo, index_t >
                build_duplicated_vertices(
                    SurfaceMesh3D& surface, index_t mesh )
            {
                const auto duplicated_vertices =
                    load_duplicated_vertices( assimp_mesh( mesh ) );
                return build_unique_vertices( duplicated_vertices, surface );
            }

            void build_polygons( SurfaceMesh3D& surface,
                const geode::NNSearch3D::ColocatedInfo& vertex_mapping,
                geode::index_t offset,
                geode::index_t mesh )
            {
                auto builder = geode::SurfaceMeshBuilder3D::create( surface );
                const auto* a_mesh = assimp_mesh( mesh );
                for( const auto p : geode::Range{ a_mesh->mNumFaces } )
                {
                    const auto& face = a_mesh->mFaces[p];
                    absl::FixedArray< geode::index_t > polygon_vertices(
                        face.mNumIndices );
                    for( const auto i :
                        geode::Range{ polygon_vertices.size() } )
                    {
                        polygon_vertices[i] =
                            offset
                            + vertex_mapping
                                  .colocated_mapping[face.mIndices[i]];
                    }
                    builder->create_polygon( polygon_vertices );
                }
            }

        private:
            std::vector< Point3D > load_duplicated_vertices(
                const aiMesh* paiMesh )
            {
                std::vector< Point3D > duplicated_vertices;
                duplicated_vertices.resize( paiMesh->mNumVertices );
                for( const auto v : Range{ paiMesh->mNumVertices } )
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

            std::tuple< NNSearch3D::ColocatedInfo, index_t >
                build_unique_vertices(
                    const std::vector< Point3D >& duplicated_vertices,
                    SurfaceMesh3D& surface )
            {
                const NNSearch3D colocater{ duplicated_vertices };
                const auto& vertex_mapping =
                    colocater.colocated_index_mapping( global_epsilon );

                auto builder = SurfaceMeshBuilder3D::create( surface );
                const auto offset = builder->create_vertices(
                    vertex_mapping.nb_unique_points() );
                for( const auto v : Range{ vertex_mapping.nb_unique_points() } )
                {
                    builder->set_point(
                        offset + v, vertex_mapping.unique_points[v] );
                }

                return std::make_tuple( vertex_mapping, offset );
            }

        private:
            absl::string_view file_;
            Assimp::Importer importer_;
            std::vector< aiMesh* > assimp_meshes_;
        };
    } // namespace detail
} // namespace geode
