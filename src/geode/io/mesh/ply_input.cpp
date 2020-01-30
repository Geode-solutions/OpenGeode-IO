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

#include <geode/io/mesh/private/ply_input.h>

#include <geode/geometry/nn_search.h>
#include <geode/geometry/point.h>

#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/core/geode_polygonal_surface.h>

#include <geode/io/mesh/private/assimp_input.h>

namespace
{
    class PLYInputImpl : public geode::detail::AssimpMeshInput
    {
    public:
        PLYInputImpl( absl::string_view filename,
            geode::PolygonalSurface3D& polygonal_surface )
            : geode::detail::AssimpMeshInput( filename ),
              surface_( polygonal_surface )
        {
        }

        void build_mesh() final
        {
            build_vertices();
            build_polygons();
        }

    private:
        void build_vertices()
        {
            auto builder = geode::PolygonalSurfaceBuilder3D::create( surface_ );
            builder->create_vertices( assimp_mesh()->mNumVertices );
            for( const auto v : geode::Range{ assimp_mesh()->mNumVertices } )
            {
                geode::Point3D point{ { assimp_mesh()->mVertices[v].x,
                    assimp_mesh()->mVertices[v].y,
                    assimp_mesh()->mVertices[v].z } };
                builder->set_point( v, point );
            }
        }

        void build_polygons()
        {
            auto builder = geode::PolygonalSurfaceBuilder3D::create( surface_ );
            for( const auto p : geode::Range{ assimp_mesh()->mNumFaces } )
            {
                const auto& face = assimp_mesh()->mFaces[p];
                std::vector< geode::index_t > polygon_vertices(
                    face.mNumIndices );
                for( const auto i : geode::Range{ face.mNumIndices } )
                {
                    polygon_vertices[i] = face.mIndices[i];
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
    namespace detail
    {
        void PLYInput::do_read()
        {
            PLYInputImpl impl{ filename(), polygonal_surface() };
            const auto success = impl.read_file();
            OPENGEODE_EXCEPTION( success, "[PLYInput::do_read] Invalid file \"",
                filename(), "\"" );
            impl.build_mesh();
        }
    } // namespace detail
} // namespace geode
