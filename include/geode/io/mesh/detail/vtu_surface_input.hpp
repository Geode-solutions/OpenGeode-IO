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

#include <geode/io/mesh/detail/vtu_input_impl.hpp>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class VTUSurfaceInput : public VTUInputImpl< Mesh >
        {
        protected:
            VTUSurfaceInput(
                std::string_view filename, const geode::MeshImpl& impl )
                : VTUInputImpl< Mesh >( filename, impl )
            {
            }

            void enable_triangle()
            {
                elements_.emplace( 5, 3 );
            }

            void enable_quad()
            {
                elements_.emplace( 9, 4 );
            }

        private:
            void read_vtk_cells( const pugi::xml_node& piece ) override
            {
                const auto nb_polygons =
                    this->read_attribute( piece, "NumberOfCells" );
                const auto polygon_offset =
                    build_polygons( piece, nb_polygons );
                this->builder().compute_polygon_adjacencies();
                this->read_data( piece.child( "CellData" ), polygon_offset,
                    this->mesh().polygon_attribute_manager() );
            }

            index_t build_polygons(
                const pugi::xml_node& piece, index_t nb_polygons )
            {
                const auto [polygon_vertices, types] =
                    this->read_cells( piece, nb_polygons );
                const auto polygons_offset = this->mesh().nb_polygons();
                for( const auto p : Indices{ polygon_vertices } )
                {
                    const auto it = elements_.find( types[p] );
                    if( it != elements_.end()
                        && it->second == polygon_vertices[p].size() )
                    {
                        this->builder().create_polygon( polygon_vertices[p] );
                    }
                }
                return polygons_offset;
            }

        private:
            absl::flat_hash_map< int64_t, local_index_t > elements_;
        };
    } // namespace detail
} // namespace geode
