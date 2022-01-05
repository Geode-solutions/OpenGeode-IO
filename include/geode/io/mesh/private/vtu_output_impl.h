/*
 * Copyright (c) 2019 - 2021 Geode-solutions
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

#include <geode/io/mesh/private/vtk_mesh_output.h>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class VTUOutputImpl : public VTKMeshOutputImpl< Mesh >
        {
        protected:
            VTUOutputImpl( absl::string_view filename, const Mesh& solid )
                : VTKMeshOutputImpl< Mesh >(
                    filename, solid, "UnstructuredGrid" )
            {
            }

        private:
            void append_number_elements( pugi::xml_node& piece ) override
            {
                piece.append_attribute( "NumberOfCells" )
                    .set_value( this->mesh().nb_polyhedra() );
            }

            void write_vtk_cells( pugi::xml_node& piece ) override
            {
                const auto nb_cells = this->mesh().nb_polyhedra();
                std::string cell_connectivity;
                cell_connectivity.reserve( nb_cells * 4 );
                std::string cell_offsets;
                cell_offsets.reserve( nb_cells );
                std::string cell_types;
                cell_types.reserve( nb_cells );
                std::string cell_faces;
                cell_faces.reserve( nb_cells * 4 );
                std::string cell_face_offsets;
                cell_face_offsets.reserve( nb_cells );
                index_t vertex_offset{ 0 };
                index_t face_offset{ 0 };
                for( const auto p : Range{ nb_cells } )
                {
                    const auto nb_vertices =
                        this->mesh().nb_polyhedron_vertices( p );
                    vertex_offset += nb_vertices;
                    absl::StrAppend( &cell_offsets, vertex_offset, " " );
                    for( const auto v : LRange{ nb_vertices } )
                    {
                        absl::StrAppend( &cell_connectivity,
                            this->mesh().polyhedron_vertex( { p, v } ), " " );
                    }
                    write_cell( p, cell_types, cell_faces, cell_face_offsets,
                        face_offset );
                }

                const auto nb_vertices = this->mesh().nb_vertices();
                auto cells = piece.append_child( "Cells" );
                auto connectivity = cells.append_child( "DataArray" );
                connectivity.append_attribute( "type" ).set_value( "Int64" );
                connectivity.append_attribute( "Name" ).set_value(
                    "connectivity" );
                connectivity.append_attribute( "format" ).set_value( "ascii" );
                connectivity.append_attribute( "RangeMin" ).set_value( 0 );
                connectivity.append_attribute( "RangeMax" )
                    .set_value( nb_vertices - 1 );
                connectivity.text().set( cell_connectivity.c_str() );

                auto offsets = cells.append_child( "DataArray" );
                offsets.append_attribute( "type" ).set_value( "Int64" );
                offsets.append_attribute( "Name" ).set_value( "offsets" );
                offsets.append_attribute( "format" ).set_value( "ascii" );
                offsets.append_attribute( "RangeMin" ).set_value( 0 );
                offsets.append_attribute( "RangeMax" ).set_value( nb_vertices );
                offsets.text().set( cell_offsets.c_str() );

                auto types = cells.append_child( "DataArray" );
                types.append_attribute( "type" ).set_value( "UInt8" );
                types.append_attribute( "Name" ).set_value( "types" );
                types.append_attribute( "format" ).set_value( "ascii" );
                types.append_attribute( "RangeMin" ).set_value( 1 );
                types.append_attribute( "RangeMax" ).set_value( 42 );
                types.text().set( cell_types.c_str() );

                if( !cell_faces.empty() )
                {
                    auto faces = cells.append_child( "DataArray" );
                    faces.append_attribute( "type" ).set_value( "Int64" );
                    faces.append_attribute( "Name" ).set_value( "faces" );
                    faces.append_attribute( "format" ).set_value( "ascii" );
                    faces.append_attribute( "RangeMin" ).set_value( 0 );
                    faces.append_attribute( "RangeMax" )
                        .set_value( nb_vertices );
                    faces.text().set( cell_faces.c_str() );
                }
                if( !cell_face_offsets.empty() )
                {
                    auto face_offsets = cells.append_child( "DataArray" );
                    face_offsets.append_attribute( "type" ).set_value(
                        "Int64" );
                    face_offsets.append_attribute( "Name" ).set_value(
                        "faceoffsets" );
                    face_offsets.append_attribute( "format" )
                        .set_value( "ascii" );
                    face_offsets.append_attribute( "RangeMin" ).set_value( -1 );
                    const std::vector< absl::string_view > tokens =
                        absl::StrSplit( cell_faces, " " );
                    face_offsets.append_attribute( "RangeMax" )
                        .set_value( tokens.size() );
                    face_offsets.text().set( cell_face_offsets.c_str() );
                }
            }

            virtual void write_cell( geode::index_t c,
                std::string& cell_types,
                std::string& cell_faces,
                std::string& cell_face_offsets,
                index_t& face_offset ) const = 0;

            void write_vtk_cell_attributes( pugi::xml_node& piece ) override
            {
                auto cell_data = piece.append_child( "CellData" );
                const auto names = this->mesh()
                                       .polyhedron_attribute_manager()
                                       .attribute_names();
                for( const auto& name : names )
                {
                    const auto attribute = this->mesh()
                                               .polyhedron_attribute_manager()
                                               .find_generic_attribute( name );
                    if( !attribute || !attribute->is_genericable() )
                    {
                        continue;
                    }
                    auto data_array = cell_data.append_child( "DataArray" );
                    data_array.append_attribute( "type" ).set_value(
                        "Float64" );
                    data_array.append_attribute( "Name" ).set_value(
                        name.data() );
                    data_array.append_attribute( "format" )
                        .set_value( "ascii" );
                    auto min = attribute->generic_value( 0 );
                    auto max = attribute->generic_value( 0 );
                    std::string values;
                    for( const auto v : Range{ this->mesh().nb_polyhedra() } )
                    {
                        const auto value = attribute->generic_value( v );
                        absl::StrAppend( &values, value, " " );
                        min = std::min( min, value );
                        max = std::max( max, value );
                    }
                    data_array.append_attribute( "RangeMin" ).set_value( min );
                    data_array.append_attribute( "RangeMax" ).set_value( max );
                    data_array.text().set( values.c_str() );
                }
            }
        };
    } // namespace detail
} // namespace geode
