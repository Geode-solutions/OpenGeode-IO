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

#include <geode/io/mesh/private/vtk_output.h>

#include <geode/basic/attribute_manager.h>

#include <geode/geometry/bounding_box.h>
#include <geode/geometry/point.h>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class VTKMeshOutputImpl : public VTKOutputImpl< Mesh >
        {
        protected:
            VTKMeshOutputImpl(
                absl::string_view filename, const Mesh& mesh, const char* type )
                : VTKOutputImpl< Mesh >{ filename, mesh, type }
            {
            }

        private:
            void write_piece( pugi::xml_node& piece ) final
            {
                piece.append_attribute( "NumberOfPoints" )
                    .set_value( this->mesh().nb_vertices() );
                append_number_elements( piece );

                write_vtk_vertex_attributes( piece );
                write_vtk_points( piece );
                write_vtk_cell_attributes( piece );
                write_vtk_cells( piece );
            }

            void write_vtk_points( pugi::xml_node& piece )
            {
                auto points = piece.append_child( "Points" );
                auto data_array = points.append_child( "DataArray" );
                data_array.append_attribute( "type" ).set_value( "Float32" );
                data_array.append_attribute( "Name" ).set_value( "Points" );
                data_array.append_attribute( "NumberOfComponents" )
                    .set_value( 3 );
                data_array.append_attribute( "format" ).set_value( "ascii" );
                const auto bbox = this->mesh().bounding_box();
                auto min = bbox.min().value( 0 );
                auto max = bbox.max().value( 0 );
                for( const auto d : Range{ 1, 3 } )
                {
                    min = std::min( min, bbox.min().value( d ) );
                    max = std::max( max, bbox.max().value( d ) );
                }
                data_array.append_attribute( "RangeMin" ).set_value( min );
                data_array.append_attribute( "RangeMax" ).set_value( max );
                std::vector< double > values;
                std::string vertices;
                for( const auto v : Range{ this->mesh().nb_vertices() } )
                {
                    absl::StrAppend(
                        &vertices, this->mesh().point( v ).string(), " " );
                    values.emplace_back( this->mesh().point( v ).value( 0 ) );
                    values.emplace_back( this->mesh().point( v ).value( 1 ) );
                    values.emplace_back( this->mesh().point( v ).value( 2 ) );
                }
                data_array.text().set( vertices.c_str() );
            }

            void write_vtk_vertex_attributes( pugi::xml_node& piece )
            {
                auto point_data = piece.append_child( "PointData" );
                const auto names =
                    this->mesh().vertex_attribute_manager().attribute_names();
                for( const auto& name : names )
                {
                    const auto attribute = this->mesh()
                                               .vertex_attribute_manager()
                                               .find_generic_attribute( name );
                    if( !attribute || !attribute->is_genericable() )
                    {
                        continue;
                    }
                    auto data_array = point_data.append_child( "DataArray" );
                    data_array.append_attribute( "type" ).set_value(
                        "Float64" );
                    data_array.append_attribute( "Name" ).set_value(
                        name.data() );
                    data_array.append_attribute( "format" )
                        .set_value( "ascii" );
                    auto min = attribute->generic_value( 0 );
                    auto max = attribute->generic_value( 0 );
                    std::string values;
                    for( const auto v :
                        geode::Range{ this->mesh().nb_vertices() } )
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

            virtual void append_number_elements( pugi::xml_node& piece ) = 0;

            virtual void write_vtk_cells( pugi::xml_node& piece ) = 0;

            virtual void write_vtk_cell_attributes( pugi::xml_node& piece ) = 0;
        };
    } // namespace detail
} // namespace geode
