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
            const auto vertex_mapping = build_dupplicated_vertices( surface_ );
            build_triangles( vertex_mapping );
        }

    private:
        void build_triangles(
            const geode::NNSearch3D::ColocatedInfo& vertex_mapping )
        {
            auto builder =
                geode::TriangulatedSurfaceBuilder3D::create( surface_ );
            for( const auto t : geode::Range{ assimp_mesh()->mNumFaces } )
            {
                std::vector< geode::index_t > triangle_vertices( 3 );
                const auto& face = assimp_mesh()->mFaces[t];
                OPENGEODE_EXCEPTION( face.mNumIndices == 3,
                    "[STLInput::build_triangles] At least one face is not a "
                    "triangle." );
                for( const auto i : geode::Range{ 3 } )
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
