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

#pragma once

#include <geode/io/image/private/vtk_output.h>

#include <geode/basic/attribute_manager.h>

#include <geode/geometry/bounding_box.h>
#include <geode/geometry/point.h>

namespace geode
{
    namespace detail
    {
        template < template < index_t > class Mesh, index_t dimension >
        class VTKMeshOutputImpl : public VTKOutputImpl< Mesh< dimension > >
        {
        protected:
            VTKMeshOutputImpl( absl::string_view filename,
                const Mesh< dimension >& mesh,
                const char* type )
                : VTKOutputImpl< Mesh< dimension > >{ filename, mesh, type }
            {
            }

        private:
            void write_piece( pugi::xml_node& object ) final
            {
                auto piece = object.append_child( "Piece" );
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
                for( const auto d : Range{ 1, dimension } )
                {
                    min = std::min( min, bbox.min().value( d ) );
                    max = std::max( max, bbox.max().value( d ) );
                }
                data_array.append_attribute( "RangeMin" ).set_value( min );
                data_array.append_attribute( "RangeMax" ).set_value( max );
                std::string vertices;
                for( const auto v : Range{ this->mesh().nb_vertices() } )
                {
                    absl::StrAppend(
                        &vertices, this->mesh().point( v ).string(), " " );
                    if( dimension < 3 )
                    {
                        absl::StrAppend( &vertices, "0 " );
                    }
                }
                data_array.text().set( vertices.c_str() );
            }

            void write_vtk_vertex_attributes( pugi::xml_node& piece )
            {
                auto point_data = piece.append_child( "PointData" );
                this->write_attributes(
                    point_data, this->mesh().vertex_attribute_manager() );
            }

            virtual void append_number_elements( pugi::xml_node& piece ) = 0;

            virtual void write_vtk_cells( pugi::xml_node& piece ) = 0;

            virtual void write_vtk_cell_attributes( pugi::xml_node& piece ) = 0;
        };
    } // namespace detail
} // namespace geode
