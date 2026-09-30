/*
 * Copyright (c) 2019 - 2026 Geode-solutions
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

#include <geode/io/mesh/detail/vtu_polyhedral_output.hpp>

#include <string>

#include <geode/mesh/core/polyhedral_solid.hpp>
#include <geode/mesh/helpers/detail/element_identifier.hpp>

#include <geode/io/mesh/detail/vtu_output_impl.hpp>

namespace
{
    class VTUPolyhedralOutputImpl
        : public geode::detail::VTUOutputImpl< geode::PolyhedralSolid >
    {
    public:
        VTUPolyhedralOutputImpl(
            std::string_view filename, const geode::PolyhedralSolid3D& solid )
            : geode::detail::VTUOutputImpl< geode::PolyhedralSolid >{ filename,
                  solid }
        {
        }

    private:
        void write_cell( geode::index_t p,
            std::vector< uint8_t >& cell_types,
            std::vector< int64_t >& cell_faces,
            std::vector< int64_t >& cell_face_offsets,
            geode::index_t& face_offset ) const override
        {
            add_cell_type( p, cell_types );
            const auto nb_faces = this->mesh().nb_polyhedron_facets( p );
            cell_faces.push_back( nb_faces );
            geode::index_t offset{ 1 };
            for( const auto f : geode::LRange{ nb_faces } )
            {
                const geode::PolyhedronFacet facet{ p, f };
                const auto nb_vertices =
                    this->mesh().nb_polyhedron_facet_vertices( facet );
                offset += nb_vertices + 1;
                cell_faces.push_back( nb_vertices );
                for( const auto v : geode::LRange{ nb_vertices } )
                {
                    cell_faces.push_back(
                        this->mesh().polyhedron_facet_vertex( { facet, v } ) );
                }
            }
            face_offset += offset;
            cell_face_offsets.push_back( face_offset );
        }

        void add_cell_type( geode::index_t polyhedron_id,
            std::vector< uint8_t >& cell_types ) const
        {
            if( geode::detail::solid_polyhedron_is_a_tetrahedron(
                    this->mesh(), polyhedron_id ) )
            {
                cell_types.push_back( geode::detail::VTK_TETRAHEDRON_TYPE );
                return;
            }
            if( geode::detail::solid_polyhedron_is_a_prism(
                    this->mesh(), polyhedron_id ) )
            {
                cell_types.push_back( geode::detail::VTK_PRISM_TYPE );
                return;
            }
            if( geode::detail::solid_polyhedron_is_a_pyramid(
                    this->mesh(), polyhedron_id ) )
            {
                cell_types.push_back( geode::detail::VTK_PYRAMID_TYPE );
                return;
            }
            if( geode::detail::solid_polyhedron_is_a_hexaedron(
                    this->mesh(), polyhedron_id ) )
            {
                cell_types.push_back( geode::detail::VTK_HEXAHEDRON_TYPE );
                return;
            }
            cell_types.push_back( geode::detail::VTK_POLYHEDRON_TYPE );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        std::vector< std::string > VTUPolyhedralOutput::write(
            const PolyhedralSolid3D& solid ) const
        {
            VTUPolyhedralOutputImpl impl{ filename(), solid };
            impl.write_file();
            return { to_string( filename() ) };
        }
    } // namespace detail
} // namespace geode
