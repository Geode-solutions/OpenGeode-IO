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

#pragma once

#include <geode/io/mesh/detail/vtu_output_impl.hpp>

#include <geode/mesh/core/hybrid_solid.hpp>
#include <geode/mesh/core/polyhedral_solid.hpp>
#include <geode/mesh/core/tetrahedral_solid.hpp>
#include <geode/mesh/helpers/detail/element_identifier.hpp>

#include <geode/io/mesh/common.hpp>

namespace geode
{
    namespace detail
    {
        class VTUTetrahedralOutputImpl
            : public VTUOutputImpl< TetrahedralSolid >
        {
        public:
            VTUTetrahedralOutputImpl(
                std::string_view filename, const TetrahedralSolid3D& solid )
                : VTUOutputImpl< TetrahedralSolid >{ filename, solid }
            {
            }

        private:
            void write_cell( index_t /*unused*/,
                std::vector< uint8_t >& cell_types,
                std::vector< int64_t >& /*unused*/,
                std::vector< int64_t >& /*unused*/,
                int64_t& /*unused*/ ) const override
            {
                cell_types.push_back( VTK_TETRAHEDRON_TYPE );
            }
        };

        class VTUHybridOutputImpl : public VTUOutputImpl< HybridSolid >
        {
        public:
            VTUHybridOutputImpl(
                std::string_view filename, const HybridSolid3D& solid )
                : VTUOutputImpl< HybridSolid >{ filename, solid }
            {
            }

        private:
            void write_cell( index_t p,
                std::vector< uint8_t >& cell_types,
                std::vector< int64_t >& /*unused*/,
                std::vector< int64_t >& /*unused*/,
                int64_t& /*unused*/ ) const override
            {
                const auto nb_vertices =
                    this->mesh().nb_polyhedron_vertices( p );
                const auto vtk_type = VTK_NB_VERTICES_TO_CELL_TYPE[nb_vertices];
                OpenGeodeIOMeshException::check_exception( vtk_type != 0,
                    nullptr, OpenGeodeException::TYPE::data,
                    "[VTUHybridOutputImpl::write_vtk_cell] Polyhedron with ",
                    nb_vertices, " vertices not supported" );
                cell_types.push_back( vtk_type );
            }
        };

        class VTUPolyhedralOutputImpl : public VTUOutputImpl< PolyhedralSolid >
        {
        public:
            VTUPolyhedralOutputImpl(
                std::string_view filename, const PolyhedralSolid3D& solid )
                : VTUOutputImpl< PolyhedralSolid >{ filename, solid }
            {
            }

        private:
            void write_cell( index_t p,
                std::vector< uint8_t >& cell_types,
                std::vector< int64_t >& cell_faces,
                std::vector< int64_t >& cell_face_offsets,
                int64_t& face_offset ) const override
            {
                add_cell_type( p, cell_types );
                const auto nb_faces = this->mesh().nb_polyhedron_facets( p );
                cell_faces.push_back( nb_faces );
                index_t offset{ 1 };
                for( const auto f : LRange{ nb_faces } )
                {
                    const PolyhedronFacet facet{ p, f };
                    const auto nb_vertices =
                        this->mesh().nb_polyhedron_facet_vertices( facet );
                    offset += nb_vertices + 1;
                    cell_faces.push_back( nb_vertices );
                    // VTK faces are oriented outward, OpenGeode facets inward
                    for( const auto v : LRange{ nb_vertices } )
                    {
                        cell_faces.push_back(
                            this->mesh().polyhedron_facet_vertex(
                                { facet, static_cast< local_index_t >(
                                             nb_vertices - 1 - v ) } ) );
                    }
                }
                face_offset += offset;
                cell_face_offsets.push_back( face_offset );
            }

            // Only tetrahedra with the VTK vertex order are written with their
            // VTK type: other polyhedra are written as VTK_POLYHEDRON, defined
            // by their faces, since their vertex order may differ from the
            // VTK one
            void add_cell_type( index_t polyhedron_id,
                std::vector< uint8_t >& cell_types ) const
            {
                if( is_vtk_tetrahedron( polyhedron_id ) )
                {
                    cell_types.push_back( VTK_TETRAHEDRON_TYPE );
                    return;
                }
                cell_types.push_back( VTK_POLYHEDRON_TYPE );
            }

            // VTK_TETRA facet opposite to vertex 3 is (0, 1, 2)
            bool is_vtk_tetrahedron( index_t polyhedron_id ) const
            {
                if( !solid_polyhedron_is_a_tetrahedron(
                        this->mesh(), polyhedron_id ) )
                {
                    return false;
                }
                std::array< index_t, 4 > vertices;
                for( const auto v : LIndices{ vertices } )
                {
                    vertices[v] =
                        this->mesh().polyhedron_vertex( { polyhedron_id, v } );
                }
                for( const auto f : LRange{
                         this->mesh().nb_polyhedron_facets( polyhedron_id ) } )
                {
                    const auto facet_vertices =
                        this->mesh().polyhedron_facet_vertices(
                            { polyhedron_id, f } );
                    if( absl::c_linear_search( facet_vertices, vertices[3] ) )
                    {
                        continue;
                    }
                    for( const auto v : LRange{ 3 } )
                    {
                        if( facet_vertices[v] == vertices[0] )
                        {
                            return facet_vertices[( v + 1 ) % 3] == vertices[1]
                                   && facet_vertices[( v + 2 ) % 3]
                                          == vertices[2];
                        }
                    }
                    return false;
                }
                return false;
            }
        };
    } // namespace detail
} // namespace geode
