/*
 * Copyright (c) 2019 - 2024 Geode-solutions
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

#include <string>

#include <geode/mesh/core/grid.h>

#include <geode/io/image/detail/vti_output_impl.h>

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        class VTIGridOutputImpl : public VTIOutputImpl< Grid< dimension > >
        {
        public:
            VTIGridOutputImpl(
                const Grid< dimension >& grid, absl::string_view filename )
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
                std::string origin_str;
                absl::StrAppend(
                    &origin_str, coordinate_system.origin().string() );
                if( dimension == 2 )
                {
                    absl::StrAppend( &origin_str, " 0" );
                }
                header_node.append_attribute( "Origin" )
                    .set_value( origin_str.c_str() );
                std::string spacing_str;
                for( const auto d : LRange{ dimension } )
                {
                    if( d != 0 )
                    {
                        absl::StrAppend( &spacing_str, " " );
                    }
                    absl::StrAppend( &spacing_str,
                        this->mesh().cell_length_in_direction( d ) );
                }
                if( dimension == 2 )
                {
                    absl::StrAppend( &spacing_str, " 1" );
                }
                header_node.append_attribute( "Spacing" )
                    .set_value( spacing_str.c_str() );
                std::string direction_str;
                for( const auto d1 : LRange{ dimension } )
                {
                    if( d1 != 0 )
                    {
                        absl::StrAppend( &direction_str, " " );
                    }
                    const auto direction =
                        coordinate_system.direction( d1 ).normalize();
                    absl::StrAppend( &direction_str, direction.string() );
                    if( dimension == 2 )
                    {
                        absl::StrAppend( &direction_str, " 0" );
                    }
                }
                if( dimension == 2 )
                {
                    absl::StrAppend( &direction_str, " 0 0 1" );
                }
                header_node.append_attribute( "Direction" )
                    .set_value( direction_str.c_str() );
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
