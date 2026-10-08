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

#include <geode/io/mesh/detail/vtk_mesh_output.hpp>

namespace geode
{
    namespace detail
    {
        static constexpr auto VTK_TRIANGLE_TYPE = 5U;
        static constexpr auto VTK_POLYGON_TYPE = 7U;
        static constexpr auto VTK_QUAD_TYPE = 9U;
        static constexpr auto VTK_TETRAHEDRON_TYPE = 10U;
        static constexpr auto VTK_HEXAHEDRON_TYPE = 12U;
        static constexpr auto VTK_PRISM_TYPE = 13U;
        static constexpr auto VTK_PYRAMID_TYPE = 14U;
        static constexpr auto VTK_POLYHEDRON_TYPE = 42U;
        static constexpr std::array< geode::index_t, 9 >
            VTK_NB_VERTICES_TO_CELL_TYPE{ 0, 0, 0, 0, VTK_TETRAHEDRON_TYPE,
                VTK_PYRAMID_TYPE, VTK_PRISM_TYPE, 0, VTK_HEXAHEDRON_TYPE };

        [[nodiscard]] constexpr index_t vtk_polygon_type( index_t nb_vertices )
        {
            if( nb_vertices == 3 )
            {
                return VTK_TRIANGLE_TYPE;
            }
            if( nb_vertices == 4 )
            {
                return VTK_QUAD_TYPE;
            }
            return VTK_POLYGON_TYPE;
        }

        template < template < index_t > class Mesh >
        class VTUOutputImpl : public VTKMeshOutputImpl< Mesh, 3 >
        {
        protected:
            VTUOutputImpl( std::string_view filename, const Mesh< 3 >& solid )
                : VTKMeshOutputImpl< Mesh, 3 >(
                      filename, solid, "UnstructuredGrid" )
            {
            }

            // Polygons written as cells after the polyhedra, defined on the
            // solid vertices
            [[nodiscard]] virtual index_t nb_additional_polygons() const
            {
                return 0;
            }

            [[nodiscard]] virtual absl::Span< const index_t >
                additional_polygon_vertices( index_t /*unused*/ ) const
            {
                return {};
            }

            // Must hold one value per polyhedron, then one per additional
            // polygon
            [[nodiscard]] virtual const AttributeManager&
                cell_attribute_manager() const
            {
                return this->mesh().polyhedron_attribute_manager();
            }

        private:
            void append_number_elements( pugi::xml_node& piece ) override
            {
                piece.append_attribute( "NumberOfCells" )
                    .set_value( this->mesh().nb_polyhedra()
                                + nb_additional_polygons() );
            }

            pugi::xml_node write_vtk_cells( pugi::xml_node& piece ) override
            {
                const auto nb_polyhedra = this->mesh().nb_polyhedra();
                const auto nb_cells = nb_polyhedra + nb_additional_polygons();
                std::vector< int64_t > cell_connectivity;
                cell_connectivity.reserve( nb_cells * 4 );
                std::vector< int64_t > cell_offsets;
                cell_offsets.reserve( nb_cells );
                std::vector< uint8_t > cell_types;
                cell_types.reserve( nb_cells );
                std::vector< int64_t > cell_faces;
                std::vector< int64_t > cell_face_offsets;
                index_t vertex_offset{ 0 };
                index_t face_offset{ 0 };
                for( const auto p : Range{ nb_polyhedra } )
                {
                    const auto nb_vertices =
                        this->mesh().nb_polyhedron_vertices( p );
                    vertex_offset += nb_vertices;
                    cell_offsets.push_back( vertex_offset );
                    for( const auto v : LRange{ nb_vertices } )
                    {
                        cell_connectivity.push_back(
                            this->mesh().polyhedron_vertex( { p, v } ) );
                    }
                    write_cell( p, cell_types, cell_faces, cell_face_offsets,
                        face_offset );
                }
                write_additional_polygons(
                    cell_connectivity, cell_offsets, cell_types );
                if( !cell_face_offsets.empty() )
                {
                    // Additional polygons are not polyhedra, they have no faces
                    cell_face_offsets.resize( cell_types.size(), -1 );
                }

                auto cells = piece.append_child( "Cells" );
                this->template write_data_array< int64_t >(
                    cells, "connectivity", cell_connectivity );
                this->template write_data_array< int64_t >(
                    cells, "offsets", cell_offsets );
                this->template write_data_array< uint8_t >(
                    cells, "types", cell_types );
                if( !cell_faces.empty() )
                {
                    this->template write_data_array< int64_t >(
                        cells, "faces", cell_faces );
                }
                if( !cell_face_offsets.empty() )
                {
                    this->template write_data_array< int64_t >(
                        cells, "faceoffsets", cell_face_offsets );
                }
                return cells;
            }

            void write_additional_polygons(
                std::vector< int64_t >& cell_connectivity,
                std::vector< int64_t >& cell_offsets,
                std::vector< uint8_t >& cell_types ) const
            {
                for( const auto polygon : Range{ nb_additional_polygons() } )
                {
                    const auto vertices =
                        additional_polygon_vertices( polygon );
                    cell_connectivity.insert( cell_connectivity.end(),
                        vertices.begin(), vertices.end() );
                    cell_offsets.push_back(
                        static_cast< int64_t >( cell_connectivity.size() ) );
                    cell_types.push_back( vtk_polygon_type( vertices.size() ) );
                }
            }

            virtual void write_cell( index_t c,
                std::vector< uint8_t >& cell_types,
                std::vector< int64_t >& cell_faces,
                std::vector< int64_t >& cell_face_offsets,
                index_t& face_offset ) const = 0;

            pugi::xml_node write_vtk_cell_attributes(
                pugi::xml_node& piece ) override
            {
                auto cell_data = piece.append_child( "CellData" );
                this->write_attributes( cell_data, cell_attribute_manager() );
                return cell_data;
            }
        };
    } // namespace detail
} // namespace geode
