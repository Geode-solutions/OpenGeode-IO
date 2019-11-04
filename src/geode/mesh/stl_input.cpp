/*
 * Copyright (c) 2019 Geode-solutions
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

#include <geode/mesh/detail/stl_input.h>

#include <geode/geometry/nn_search.h>
#include <geode/geometry/point.h>

#include <geode/mesh/builder/triangulated_surface_builder.h>
#include <geode/mesh/core/geode_triangulated_surface.h>
#include <geode/mesh/detail/assimp_input.h>

namespace
{
    class STLInputImpl : public geode::detail::AssimpMeshInput
    {
    public:
        STLInputImpl( std::string filename,
            geode::TriangulatedSurface3D& triangulated_surface )
            : geode::detail::AssimpMeshInput( std::move( filename ) ),
              surface_( triangulated_surface )
        {
        }
        void build_mesh()
        {
            auto vertex_mapping = build_vertices();
            build_triangles( vertex_mapping );
        }

    private:
        geode::NNSearch3D::ColocatedInfo build_vertices()
        {
            auto duplicated_vertices =
                load_duplicated_vertices( assimp_mesh() );
            return build_unique_vertices( duplicated_vertices );
        }

        std::vector< geode::Point3D > load_duplicated_vertices(
            const aiMesh* paiMesh )
        {
            std::vector< geode::Point3D > duplicated_vertices;
            duplicated_vertices.resize( paiMesh->mNumVertices );
            for( auto v : geode::Range{ paiMesh->mNumVertices } )
            {
                duplicated_vertices[v].set_value( 0, paiMesh->mVertices[v].x );
                duplicated_vertices[v].set_value( 1, paiMesh->mVertices[v].y );
                duplicated_vertices[v].set_value( 2, paiMesh->mVertices[v].z );
            }
            return duplicated_vertices;
        }

        geode::NNSearch3D::ColocatedInfo build_unique_vertices(
            const std::vector< geode::Point3D >& duplicated_vertices )
        {
            geode::NNSearch3D colocater{ duplicated_vertices };
            const auto& vertex_mapping =
                colocater.colocated_index_mapping( geode::global_epsilon );

            auto builder =
                geode::TriangulatedSurfaceBuilder3D::create( surface_ );
            builder->create_vertices( vertex_mapping.nb_unique_points() );
            for( auto v : geode::Range{ vertex_mapping.nb_unique_points() } )
            {
                builder->set_point( v, vertex_mapping.unique_points[v] );
            }

            return vertex_mapping;
        }

        void build_triangles(
            const geode::NNSearch3D::ColocatedInfo& vertex_mapping )
        {
            auto builder =
                geode::TriangulatedSurfaceBuilder3D::create( surface_ );
            for( auto t : geode::Range{ assimp_mesh()->mNumFaces } )
            {
                std::vector< geode::index_t > triangle_vertices( 3 );
                const auto& face = assimp_mesh()->mFaces[t];
                OPENGEODE_EXCEPTION( face.mNumIndices == 3,
                    "[STLInput::build_triangles] At least one face is not a "
                    "triangle." );
                for( auto i : geode::Range{ 3 } )
                {
                    triangle_vertices[i] =
                        vertex_mapping.colocated_mapping[face.mIndices[i]];
                }
                builder->create_polygon( triangle_vertices );
            }
            builder->compute_polygon_adjacencies();
        }

    private:
        std::string file_;
        geode::TriangulatedSurface3D& surface_;
        Assimp::Importer importer_;
    };
} // namespace

namespace geode
{
    void STLInput::do_read()
    {
        STLInputImpl impl{ filename(), triangulated_surface() };
        auto success = impl.read_file();
        OPENGEODE_EXCEPTION( success,
            "[STLInput::do_read] Invalid file \"" + filename() + "\"" );
        impl.build_mesh();
    }
} // namespace geode
