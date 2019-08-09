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

#include <geode/mesh/detail/obj_input.h>

#include <geode/basic/nn_search.h>
#include <geode/basic/point.h>

#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/core/geode_polygonal_surface.h>
#include <geode/mesh/detail/assimp_input.h>

namespace
{
    class OBJInputImpl : public geode::detail::AssimpMeshInput
    {
    public:
        OBJInputImpl(
            std::string filename, geode::PolygonalSurface3D& polygonal_surface )
            : geode::detail::AssimpMeshInput( std::move( filename ) ),
              surface_( polygonal_surface )
        {
        }

        void build_mesh()
        {
            auto vertex_mapping = build_vertices();
            build_polygons( vertex_mapping );
        }

    private:
        geode::NNSearch3D::ColocatedInfo build_vertices()
        {
            auto dupplicated_vertices =
                load_dupplicated_vertices( assimp_mesh() );
            return build_unique_vertices( dupplicated_vertices );
        }

        std::vector< geode::Point3D > load_dupplicated_vertices(
            const aiMesh* paiMesh )
        {
            std::vector< geode::Point3D > dupplicated_vertices;
            dupplicated_vertices.resize( paiMesh->mNumVertices );
            for( auto v : geode::Range{ paiMesh->mNumVertices } )
            {
                dupplicated_vertices[v].set_value( 0, paiMesh->mVertices[v].x );
                dupplicated_vertices[v].set_value( 1, paiMesh->mVertices[v].y );
                dupplicated_vertices[v].set_value( 2, paiMesh->mVertices[v].z );
            }
            return dupplicated_vertices;
        }

        geode::NNSearch3D::ColocatedInfo build_unique_vertices(
            const std::vector< geode::Point3D >& dupplicated_vertices )
        {
            geode::NNSearch3D colocater{ dupplicated_vertices };
            const auto& vertex_mapping =
                colocater.colocated_index_mapping( geode::global_epsilon );

            auto builder =
                geode::PolygonalSurfaceBuilder3D::create( surface_ );
            builder->create_vertices( vertex_mapping.nb_unique_points() );
            for( auto v : geode::Range{ vertex_mapping.nb_unique_points() } )
            {
                builder->set_point( v, vertex_mapping.unique_points[v] );
            }

            return vertex_mapping;
        }

        void build_polygons(const geode::NNSearch3D::ColocatedInfo& vertex_mapping )
        {
            auto builder = geode::PolygonalSurfaceBuilder3D::create( surface_ );
            for( auto p : geode::Range{ assimp_mesh()->mNumFaces } )
            {
                const auto& face = assimp_mesh()->mFaces[p];
                std::vector< geode::index_t > polygon_vertices( face.mNumIndices );
                for( auto i : geode::Range{ polygon_vertices.size() } )
                {
                    polygon_vertices[i] =
                        vertex_mapping.colocated_mapping[face.mIndices[i]];
                }
                builder->create_polygon( polygon_vertices );
            }
            builder->compute_polygon_adjacencies();
        }

    private:
        geode::PolygonalSurface3D& surface_;
    };
} // namespace

namespace geode
{
    void OBJInput::do_read()
    {
        OBJInputImpl impl{ filename(), polygonal_surface() };
        impl.read_file();
        impl.build_mesh();
    }
} // namespace geode
