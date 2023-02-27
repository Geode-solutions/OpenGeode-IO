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

        template < index_t dimension >
        inline void write_point(
            std::string& string, const Point< dimension >& point )
        {
            if( dimension < 3 )
            {
                absl::StrAppend( &string, point.string(), " 0 " );
            }
            else
            {
                absl::StrAppend( &string, point.string(), " " );
            }
        }

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

            virtual std::vector< index_t > compute_vertices()
            {
                std::vector< index_t > vertices( this->mesh().nb_vertices() );
                absl::c_iota( vertices, 0 );
                return vertices;
            }

        private:
            void write_piece( pugi::xml_node& object ) final
            {
                auto piece = object.append_child( "Piece" );
                const auto vertices = compute_vertices();
                piece.append_attribute( "NumberOfPoints" )
                    .set_value( vertices.size() );
                append_number_elements( piece );

                auto vertex_node =
                    write_vtk_vertex_attributes( piece, vertices );
                write_vtk_textures( vertex_node );
                write_vtk_points( piece, vertices );
                write_vtk_cell_attributes( piece );
                write_vtk_cells( piece );
            }

            pugi::xml_node write_vtk_points( pugi::xml_node& piece,
                absl::Span< const index_t > vertices ) const
            {
                auto points = piece.append_child( "Points" );
                auto data_array =
                    this->write_attribute_header( points, "Points", 3 );
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
                std::string vertices_str;
                for( const auto v : vertices )
                {
                    write_point( vertices_str, this->mesh().point( v ) );
                }
                data_array.text().set( vertices_str.c_str() );
                return points;
            }

            pugi::xml_node write_vtk_vertex_attributes( pugi::xml_node& piece,
                absl::Span< const index_t > vertices ) const
            {
                auto point_data = piece.append_child( "PointData" );
                this->write_attributes( point_data,
                    this->mesh().vertex_attribute_manager(), vertices );
                return point_data;
            }

            virtual void append_number_elements( pugi::xml_node& piece ) = 0;

            virtual void write_vtk_textures( pugi::xml_node& /*unused*/ ){};

            virtual pugi::xml_node write_vtk_cells( pugi::xml_node& piece ) = 0;

            virtual pugi::xml_node write_vtk_cell_attributes(
                pugi::xml_node& piece ) = 0;
        };
    } // namespace detail
} // namespace geode
