/*
 * Copyright (c) 2019 - 2025 Geode-solutions
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
            static GridAttributes read_grid_attributes(
                const pugi::xml_node& vtk_object )
            {
                GridAttributes grid_attributes;
                for( const auto& attribute : vtk_object.attributes() )
                {
                    if( std::strcmp( attribute.name(), "WholeExtent" ) == 0 )
                    {
                        const auto tokens = string_split( attribute.value() );
                        for( const auto d : LRange{ dimension } )
                        {
                            const auto start = string_to_index( tokens[2 * d] );
                            const auto end =
                                string_to_index( tokens[2 * d + 1] );
                            grid_attributes.cells_number[d] = end - start;
                        }
                    }
                    else if( std::strcmp( attribute.name(), "Origin" ) == 0 )
                    {
                        const auto tokens = string_split( attribute.value() );
                        for( const auto d : LRange{ dimension } )
                        {
                            grid_attributes.origin.set_value(
                                d, string_to_double( tokens[d] ) );
                        }
                    }
                    else if( std::strcmp( attribute.name(), "Spacing" ) == 0 )
                    {
                        const auto tokens = string_split( attribute.value() );
                        for( const auto d : LRange{ dimension } )
                        {
                            grid_attributes.cells_length[d] =
                                string_to_double( tokens[d] );
                        }
                    }
                    else if( std::strcmp( attribute.name(), "Direction" ) == 0 )
                    {
                        const auto tokens = string_split( attribute.value() );
                        for( const auto d : LRange{ dimension } )
                        {
                            auto& direction =
                                grid_attributes.cell_directions[d];
                            for( const auto i : LRange{ dimension } )
                            {
                                direction.set_value(
                                    i, string_to_double( tokens[3 * d + i] ) );
                            }
                        }
                    }
                }
                for( const auto d : LRange{ dimension } )
                {
                    grid_attributes.cell_directions[d] *=
                        grid_attributes.cells_length[d];
                }
                return grid_attributes;
            }

        private:
            void read_vtk_object( const pugi::xml_node& vtk_object ) final
            {
                build_grid( vtk_object );
                for( const auto& piece : vtk_object.children( "Piece" ) )
                {
                    this->read_data( piece.child( "PointData" ), 0,
                        this->mesh().grid_vertex_attribute_manager() );
                    this->read_data( piece.child( "CellData" ), 0,
                        this->mesh().cell_attribute_manager() );
                }
            }

            bool is_vtk_object_loadable(
                const pugi::xml_node& vtk_object ) const final
            {
                const auto grid_attributes = read_grid_attributes( vtk_object );
                const auto nb_cells_3d = grid_attributes.cells_number[2];
                return dimension == 2 ? nb_cells_3d == 0 : nb_cells_3d > 0;
            }

        protected:
            virtual void build_grid( const pugi::xml_node& vtk_object ) = 0;
        };
    } // namespace detail
} // namespace geode
