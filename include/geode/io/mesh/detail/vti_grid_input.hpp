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

#include <array>
#include <fstream>
#include <optional>

#include <absl/strings/numbers.h>

#include <geode/basic/string.hpp>

#include <geode/geometry/vector.hpp>

#include <geode/mesh/core/grid.hpp>

#include <geode/io/mesh/detail/vtk_input.hpp>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class VTIGridInputImpl : public VTKInputImpl< Mesh >
        {
        public:
            static constexpr auto dimension = Mesh::dim;

            explicit VTIGridInputImpl( std::string_view filename )
                : VTKInputImpl< Mesh >{ filename, "ImageData" }
            {
            }

            struct GridAttributes
            {
                GridAttributes()
                {
                    cells_number.fill( 0 );
                    cells_length.fill( 1 );
                    for( const auto d : LRange{ dimension } )
                    {
                        cell_directions[d].set_value( d, 1 );
                    }
                }

                Point< dimension > origin;
                std::array< index_t, dimension > cells_number;
                std::array< double, dimension > cells_length;
                std::array< Vector< dimension >, dimension > cell_directions;
            };

        protected:
            using Extent = std::array< std::array< int64_t, 2 >, 3 >;

            // Reads the [start, end] point indices in each direction, nullopt
            // if WholeExtent is missing or malformed
            static std::optional< Extent > read_whole_extent(
                const pugi::xml_node& vtk_object )
            {
                const auto tokens = string_split(
                    vtk_object.attribute( "WholeExtent" ).value() );
                if( tokens.size() != 6 )
                {
                    return std::nullopt;
                }
                Extent extent;
                for( const auto d : LRange{ 3 } )
                {
                    for( const auto i : LRange{ 2 } )
                    {
                        if( !absl::SimpleAtoi(
                                tokens[2 * d + i], &extent[d][i] ) )
                        {
                            return std::nullopt;
                        }
                    }
                    if( extent[d][1] < extent[d][0] )
                    {
                        return std::nullopt;
                    }
                }
                return extent;
            }

            static GridAttributes read_grid_attributes(
                const pugi::xml_node& vtk_object )
            {
                GridAttributes grid_attributes;
                const auto extent = read_whole_extent( vtk_object );
                OpenGeodeIOMeshException::check_exception( extent.has_value(),
                    nullptr, OpenGeodeException::TYPE::data,
                    "[VTIInput::read_grid_attributes] Missing or malformed "
                    "WholeExtent: ",
                    vtk_object.attribute( "WholeExtent" ).value() );
                for( const auto d : LRange{ dimension } )
                {
                    const auto nb_cells =
                        extent.value()[d][1] - extent.value()[d][0];
                    OpenGeodeIOMeshException::check_exception( nb_cells < NO_ID,
                        nullptr, OpenGeodeException::TYPE::data,
                        "[VTIInput::read_grid_attributes] Too many cells in "
                        "direction ",
                        d );
                    grid_attributes.cells_number[d] =
                        static_cast< index_t >( nb_cells );
                }
                if( const auto origin = vtk_object.attribute( "Origin" ) )
                {
                    const auto tokens =
                        read_values( origin.value(), dimension, "Origin" );
                    for( const auto d : LRange{ dimension } )
                    {
                        grid_attributes.origin.set_value( d, tokens[d] );
                    }
                }
                if( const auto spacing = vtk_object.attribute( "Spacing" ) )
                {
                    const auto tokens =
                        read_values( spacing.value(), dimension, "Spacing" );
                    for( const auto d : LRange{ dimension } )
                    {
                        grid_attributes.cells_length[d] = tokens[d];
                    }
                }
                if( const auto direction_attribute =
                        vtk_object.attribute( "Direction" ) )
                {
                    const auto tokens =
                        read_values( direction_attribute.value(), 3 * dimension,
                            "Direction" );
                    for( const auto d : LRange{ dimension } )
                    {
                        auto& direction = grid_attributes.cell_directions[d];
                        for( const auto i : LRange{ dimension } )
                        {
                            direction.set_value( i, tokens[3 * d + i] );
                        }
                    }
                }
                for( const auto d : LRange{ dimension } )
                {
                    grid_attributes.cell_directions[d] *=
                        grid_attributes.cells_length[d];
                }
                // Points are numbered from the extent start, not from 0
                for( const auto d : LRange{ dimension } )
                {
                    grid_attributes.origin +=
                        grid_attributes.cell_directions[d]
                        * static_cast< double >( extent.value()[d][0] );
                }
                return grid_attributes;
            }

            static std::vector< double > read_values( std::string_view value,
                index_t nb_values,
                std::string_view attribute_name )
            {
                const auto tokens = string_split( value );
                OpenGeodeIOMeshException::check_exception(
                    tokens.size() >= nb_values, nullptr,
                    OpenGeodeException::TYPE::data,
                    "[VTIInput::read_grid_attributes] Expected ", nb_values,
                    " values for ", attribute_name, ", got ", tokens.size() );
                std::vector< double > values;
                values.reserve( tokens.size() );
                for( const auto& token : tokens )
                {
                    values.push_back( string_to_double( token ) );
                }
                return values;
            }

        private:
            void read_vtk_object( const pugi::xml_node& vtk_object ) final
            {
                build_grid( vtk_object );
                for( const auto& piece : vtk_object.children( "Piece" ) )
                {
                    this->read_data( piece.child( "PointData" ),
                        DataMapping{ this->mesh().nb_grid_vertices(), 0 },
                        this->mesh().grid_vertex_attribute_manager() );
                    this->read_data( piece.child( "CellData" ),
                        DataMapping{ this->mesh().nb_cells(), 0 },
                        this->mesh().cell_attribute_manager() );
                }
            }

            void is_vtk_object_loadable( const pugi::xml_node& vtk_object,
                std::vector< Percentage >& percentages ) const final
            {
                const auto extent = read_whole_extent( vtk_object );
                if( !extent )
                {
                    percentages.emplace_back( 0 );
                    return;
                }
                const auto nb_cells_3d =
                    extent.value()[2][1] - extent.value()[2][0];
                percentages.emplace_back(
                    dimension == 2 ? nb_cells_3d == 0 : nb_cells_3d > 0 );
            }

        protected:
            virtual void build_grid( const pugi::xml_node& vtk_object ) = 0;
        };
    } // namespace detail
} // namespace geode
