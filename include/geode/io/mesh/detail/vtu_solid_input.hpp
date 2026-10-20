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

#include <absl/container/inlined_vector.h>

#include <geode/io/mesh/detail/vtu_input_impl.hpp>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class VTUSolidInput : public VTUInputImpl< Mesh >
        {
            struct VTKElement
            {
                VTKElement( local_index_t nb_vertices_in,
                    std::vector< std::vector< local_index_t > > facets_in,
                    std::vector< local_index_t > vertex_order_in = {} )
                    : nb_vertices{ nb_vertices_in },
                      facets{ std::move( facets_in ) },
                      vertex_order{ std::move( vertex_order_in ) }
                {
                }

                local_index_t nb_vertices;
                std::vector< std::vector< local_index_t > > facets;
                // Position in the VTK cell of each polyhedron vertex, empty
                // if the vertex orders are the same
                std::vector< local_index_t > vertex_order;
            };

        protected:
            VTUSolidInput(
                std::string_view filename, const geode::MeshImpl& impl )
                : VTUInputImpl< Mesh >( filename, impl )
            {
            }

            void enable_tetrahedron()
            {
                elements_.emplace(
                    10, VTKElement{ 4, { { 1, 3, 2 }, { 0, 2, 3 }, { 3, 1, 0 },
                                           { 0, 1, 2 } } } );
            }

            void enable_hexahedron()
            {
                // VTK_HEXAHEDRON vertex order is the OpenGeode one
                const std::vector< std::vector< local_index_t > > facets{
                    { 0, 4, 5, 1 }, { 2, 6, 7, 3 }, { 0, 3, 7, 4 },
                    { 1, 5, 6, 2 }, { 0, 1, 2, 3 }, { 4, 7, 6, 5 }
                };
                elements_.emplace( 12, VTKElement{ 8, facets } );
                // VTK_VOXEL vertices are ordered along x, then y, then z
                elements_.emplace(
                    11, VTKElement{ 8, facets, { 0, 1, 3, 2, 4, 5, 7, 6 } } );
            }

            void enable_prism()
            {
                elements_.emplace( 13,
                    VTKElement{ 6, { { 0, 2, 1 }, { 3, 4, 5 }, { 0, 3, 5, 2 },
                                       { 1, 2, 5, 4 }, { 0, 1, 4, 3 } } } );
            }

            void enable_pyramid()
            {
                elements_.emplace(
                    14, VTKElement{ 5, { { 0, 4, 1 }, { 1, 4, 2 }, { 2, 4, 3 },
                                           { 3, 4, 0 }, { 0, 1, 2, 3 } } } );
            }

        private:
            void read_vtk_cells( const pugi::xml_node& piece ) override
            {
                const auto nb_polyhedra =
                    this->read_attribute( piece, "NumberOfCells" );
                const auto cell_polyhedra =
                    build_polyhedra( piece, nb_polyhedra );
                this->builder().compute_polyhedron_adjacencies();
                this->read_data( piece.child( "CellData" ),
                    DataMapping{ cell_polyhedra },
                    this->mesh().polyhedron_attribute_manager() );
            }

            Percentage is_vtk_cells_loadable(
                const pugi::xml_node& piece ) const override
            {
                const auto nb_polyhedra =
                    this->read_attribute( piece, "NumberOfCells" );
                if( nb_polyhedra == 0 )
                {
                    return Percentage{ 1 };
                }
                const auto [_, types] = this->read_cells( piece, nb_polyhedra );
                index_t nb_loadable_polyhedra{ 0 };
                for( const auto& type : types )
                {
                    if( elements_.contains( type ) )
                    {
                        nb_loadable_polyhedra++;
                    }
                }
                return Percentage{ static_cast< double >(
                                       nb_loadable_polyhedra )
                                   / nb_polyhedra };
            }

            // Returns the polyhedron created for each cell, NO_ID if none
            std::vector< index_t > build_polyhedra(
                const pugi::xml_node& piece, index_t nb_polyhedra )
            {
                const auto [polyhedron_vertices, types] =
                    this->read_cells( piece, nb_polyhedra );
                std::vector< index_t > cell_polyhedra(
                    polyhedron_vertices.size(), NO_ID );
                index_t nb_invalid_polyhedra{ 0 };
                index_t nb_wrong_size_polyhedra{ 0 };
                for( const auto p : Indices{ polyhedron_vertices } )
                {
                    const auto it = elements_.find( types[p] );
                    if( it == elements_.end() )
                    {
                        continue;
                    }
                    const auto& element = it->second;
                    if( element.nb_vertices != polyhedron_vertices[p].size() )
                    {
                        nb_wrong_size_polyhedra++;
                        continue;
                    }
                    if( !this->are_cell_vertices_valid(
                            polyhedron_vertices[p] ) )
                    {
                        nb_invalid_polyhedra++;
                        continue;
                    }
                    cell_polyhedra[p] =
                        create_polyhedron( polyhedron_vertices[p], element );
                }
                this->warn_invalid_cells( nb_invalid_polyhedra );
                this->warn_wrong_size_cells( nb_wrong_size_polyhedra );
                return cell_polyhedra;
            }

            index_t create_polyhedron( absl::Span< const index_t > vertices,
                const VTKElement& element )
            {
                if( element.vertex_order.empty() )
                {
                    return this->builder().create_polyhedron(
                        vertices, element.facets );
                }
                absl::InlinedVector< index_t, 8 > ordered_vertices;
                for( const auto vertex : element.vertex_order )
                {
                    ordered_vertices.push_back( vertices[vertex] );
                }
                return this->builder().create_polyhedron(
                    ordered_vertices, element.facets );
            }

        private:
            absl::flat_hash_map< int64_t, VTKElement > elements_;
        };
    } // namespace detail
} // namespace geode
