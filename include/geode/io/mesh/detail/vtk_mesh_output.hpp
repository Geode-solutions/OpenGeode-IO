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

#include <geode/io/image/detail/vtk_output.hpp>

#include <geode/basic/attribute_manager.hpp>

#include <geode/geometry/point.hpp>

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        inline void write_point( std::vector< double >& coordinates,
            const Point< dimension >& point )
        {
            for( const auto d : LRange{ dimension } )
            {
                coordinates.push_back( point.value( d ) );
            }
            for( [[maybe_unused]] const auto d : LRange{ dimension, 3 } )
            {
                coordinates.push_back( 0 );
            }
        }

        template < template < index_t > class Mesh, index_t dimension >
        class VTKMeshOutputImpl : public VTKOutputImpl< Mesh< dimension > >
        {
        protected:
            VTKMeshOutputImpl( std::string_view filename,
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

            pugi::xml_node write_vtk_points(
                pugi::xml_node& piece, absl::Span< const index_t > vertices )
            {
                auto points = piece.append_child( "Points" );
                if( vertices.size() == 0 )
                {
                    return points;
                }
                std::vector< double > coordinates;
                coordinates.reserve( 3 * vertices.size() );
                for( const auto v : vertices )
                {
                    write_point( coordinates, this->mesh().point( v ) );
                }
                this->template write_data_array< double >(
                    points, "Points", coordinates, 3 );
                return points;
            }

            pugi::xml_node write_vtk_vertex_attributes(
                pugi::xml_node& piece, absl::Span< const index_t > vertices )
            {
                auto point_data = piece.append_child( "PointData" );
                this->write_attributes( point_data,
                    this->mesh().vertex_attribute_manager(), vertices );
                return point_data;
            }

            virtual void append_number_elements( pugi::xml_node& piece ) = 0;

            virtual void write_vtk_textures( pugi::xml_node& /*unused*/ ) {};

            virtual pugi::xml_node write_vtk_cells( pugi::xml_node& piece ) = 0;

            virtual pugi::xml_node write_vtk_cell_attributes(
                pugi::xml_node& piece ) = 0;
        };
    } // namespace detail
} // namespace geode
