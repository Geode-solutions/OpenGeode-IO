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

#include <string>

#include <geode/geometry/coordinate_system.hpp>

#include <geode/mesh/core/grid.hpp>

#include <geode/io/image/detail/vti_output_impl.hpp>

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        class VTIGridOutputImpl : public VTIOutputImpl< Grid< dimension > >
        {
        public:
            VTIGridOutputImpl(
                const Grid< dimension >& grid, std::string_view filename )
                : VTIOutputImpl< Grid< dimension > >{ grid, filename }
            {
            }

        private:
            void write_piece( pugi::xml_node& object ) final
            {
                auto piece = object.append_child( "Piece" );
                std::array< index_t, dimension > extent;
                for( const auto d : LRange{ dimension } )
                {
                    extent[d] = this->mesh().nb_vertices_in_direction( d );
                }
                auto header_node = this->write_image_header( piece, extent );
                write_header( header_node );
                write_vertex_data( piece );
                write_cell_data( piece );
            }

            void write_header( pugi::xml_node& header_node )
            {
                const auto& coordinate_system =
                    this->mesh().grid_coordinate_system();
                std::vector< double > origin;
                std::vector< double > spacing;
                std::vector< double > directions;
                for( const auto d : LRange{ dimension } )
                {
                    origin.push_back( coordinate_system.origin().value( d ) );
                    spacing.push_back(
                        this->mesh().cell_length_in_direction( d ) );
                    const auto direction =
                        coordinate_system.direction( d ).normalize();
                    for( const auto i : LRange{ dimension } )
                    {
                        directions.push_back( direction.value( i ) );
                    }
                    if( dimension == 2 )
                    {
                        directions.push_back( 0 );
                    }
                }
                if( dimension == 2 )
                {
                    origin.push_back( 0 );
                    spacing.push_back( 1 );
                    directions.insert( directions.end(), { 0, 0, 1 } );
                }
                header_node.append_attribute( "Origin" )
                    .set_value( to_vtk_string( origin ).c_str() );
                header_node.append_attribute( "Spacing" )
                    .set_value( to_vtk_string( spacing ).c_str() );
                header_node.append_attribute( "Direction" )
                    .set_value( to_vtk_string( directions ).c_str() );
            }

            static std::string to_vtk_string(
                absl::Span< const double > values )
            {
                std::string result;
                for( const auto value : values )
                {
                    if( !result.empty() )
                    {
                        result.push_back( ' ' );
                    }
                    result.append( vtk_double_string( value ) );
                }
                return result;
            }

            void write_cell_data( pugi::xml_node& piece )
            {
                auto cell_data = piece.append_child( "CellData" );
                this->write_attributes(
                    cell_data, this->mesh().cell_attribute_manager() );
            }

            void write_vertex_data( pugi::xml_node& piece )
            {
                auto vertex_data = piece.append_child( "PointData" );
                this->write_attributes(
                    vertex_data, this->mesh().grid_vertex_attribute_manager() );
            }
        };
    } // namespace detail
} // namespace geode
