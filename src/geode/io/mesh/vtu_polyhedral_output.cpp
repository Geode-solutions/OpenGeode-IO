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

#include <geode/io/mesh/private/vtu_polyhedral_output.h>

#include <geode/mesh/core/polyhedral_solid.h>

#include <geode/io/mesh/private/vtu_output_impl.h>

namespace
{
    class VTUPolyhedralOutputImpl
        : public geode::detail::VTUOutputImpl< geode::PolyhedralSolid >
    {
    public:
        VTUPolyhedralOutputImpl(
            absl::string_view filename, const geode::PolyhedralSolid3D& solid )
            : geode::detail::VTUOutputImpl< geode::PolyhedralSolid >{ filename,
                  solid }
        {
        }

    private:
        void write_cell( geode::index_t p,
            std::string& cell_types,
            std::string& cell_faces,
            std::string& cell_face_offsets,
            geode::index_t& face_offset ) const override
        {
            absl::StrAppend( &cell_types, "42 " );
            const auto nb_faces = this->mesh().nb_polyhedron_facets( p );
            absl::StrAppend( &cell_faces, nb_faces, " " );
            geode::index_t offset{ 1 };
            for( const auto f : geode::LRange{ nb_faces } )
            {
                const geode::PolyhedronFacet facet{ p, f };
                const auto nb_vertices =
                    this->mesh().nb_polyhedron_facet_vertices( facet );
                offset += nb_vertices + 1;
                absl::StrAppend( &cell_faces, nb_vertices, " " );
                for( const auto v : geode::LRange{ nb_vertices } )
                {
                    absl::StrAppend( &cell_faces,
                        this->mesh().polyhedron_facet_vertex( { facet, v } ),
                        " " );
                }
            }
            face_offset += offset;
            absl::StrAppend( &cell_face_offsets, face_offset, " " );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void VTUPolyhedralOutput::write( const PolyhedralSolid3D& solid ) const
        {
            VTUPolyhedralOutputImpl impl{ filename(), solid };
            impl.write_file();
        }
    } // namespace detail
} // namespace geode
