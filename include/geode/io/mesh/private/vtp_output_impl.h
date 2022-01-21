/*
 * Copyright (c) 2019 - 2022 Geode-solutions
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
        template < template < index_t > class Mesh, index_t dimension >
        class VTPOutputImpl : public VTKMeshOutputImpl< Mesh, dimension >
        {
        public:
            VTPOutputImpl( absl::string_view filename,
                const Mesh< dimension >& polygonal_surface )
                : VTKMeshOutputImpl< Mesh, dimension >(
                    filename, polygonal_surface, "PolyData" )
            {
            }

        private:
            void append_number_elements( pugi::xml_node& piece ) override
            {
                piece.append_attribute( "NumberOfPolys" )
                    .set_value( this->mesh().nb_polygons() );
            }

            void write_vtk_cells( pugi::xml_node& piece ) override
            {
                auto polys = piece.append_child( "Polys" );
                auto connectivity = polys.append_child( "DataArray" );
                connectivity.append_attribute( "type" ).set_value( "Int64" );
                connectivity.append_attribute( "Name" ).set_value(
                    "connectivity" );
                connectivity.append_attribute( "format" ).set_value( "ascii" );
                connectivity.append_attribute( "RangeMin" ).set_value( 0 );
                connectivity.append_attribute( "RangeMax" )
                    .set_value( this->mesh().nb_vertices() - 1 );
                auto offsets = polys.append_child( "DataArray" );
                offsets.append_attribute( "type" ).set_value( "Int64" );
                offsets.append_attribute( "Name" ).set_value( "offsets" );
                offsets.append_attribute( "format" ).set_value( "ascii" );
                offsets.append_attribute( "RangeMin" ).set_value( 0 );
                offsets.append_attribute( "RangeMax" )
                    .set_value( this->mesh().nb_vertices() );
                const auto nb_polys = this->mesh().nb_polygons();
                std::string poly_connectivity;
                poly_connectivity.reserve( nb_polys * 3 );
                std::string poly_offsets;
                poly_offsets.reserve( nb_polys );
                index_t vertex_count{ 0 };
                for( const auto p : Range{ nb_polys } )
                {
                    const auto nb_polygon_vertices =
                        this->mesh().nb_polygon_vertices( p );
                    vertex_count += nb_polygon_vertices;
                    absl::StrAppend( &poly_offsets, vertex_count, " " );
                    for( const auto v : LRange{ nb_polygon_vertices } )
                    {
                        absl::StrAppend( &poly_connectivity,
                            this->mesh().polygon_vertex( { p, v } ), " " );
                    }
                }
                connectivity.text().set( poly_connectivity.c_str() );
                offsets.text().set( poly_offsets.c_str() );
            }

            void write_vtk_cell_attributes( pugi::xml_node& piece ) override
            {
                auto cell_data = piece.append_child( "CellData" );
                const auto names =
                    this->mesh().polygon_attribute_manager().attribute_names();
                for( const auto& name : names )
                {
                    const auto attribute = this->mesh()
                                               .polygon_attribute_manager()
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
                    for( const auto v : Range{ this->mesh().nb_polygons() } )
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
