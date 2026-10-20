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

#include <optional>

#include <absl/algorithm/container.h>
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

            // Enables VTK_POLYHEDRON cells, defined by their faces
            void enable_polyhedron()
            {
                polyhedron_enabled_ = true;
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
                    if( elements_.contains( type )
                        || ( polyhedron_enabled_
                             && type == VTK_POLYHEDRON_TYPE ) )
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
                VTKPolyhedra vtk_polyhedra{ *this, piece, types };
                for( const auto p : Indices{ polyhedron_vertices } )
                {
                    const auto faces = vtk_polyhedra.next_cell_faces( p );
                    if( types[p] == VTK_POLYHEDRON_TYPE )
                    {
                        if( !this->are_cell_vertices_valid(
                                polyhedron_vertices[p] ) )
                        {
                            nb_invalid_polyhedra++;
                            continue;
                        }
                        cell_polyhedra[p] = vtk_polyhedra.create_polyhedron(
                            polyhedron_vertices[p], faces );
                        continue;
                    }
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
                vtk_polyhedra.warn_skipped_polyhedra();
                return cell_polyhedra;
            }

            // Reads the faces of the VTK_POLYHEDRON cells of a piece
            class VTKPolyhedra
            {
            public:
                VTKPolyhedra( VTUSolidInput& input,
                    const pugi::xml_node& piece,
                    absl::Span< const uint8_t > types )
                    : input_( input )
                {
                    if( !input_.polyhedron_enabled_
                        || !absl::c_linear_search(
                            types, VTK_POLYHEDRON_TYPE ) )
                    {
                        return;
                    }
                    for( const auto& data :
                        piece.child( "Cells" ).children( "DataArray" ) )
                    {
                        const auto name = data.attribute( "Name" ).value();
                        if( input_.match( name, "faces" ) )
                        {
                            faces_ = input_.template read_data_array< int64_t >(
                                data );
                        }
                        else if( input_.match( name, "faceoffsets" ) )
                        {
                            face_offsets_ =
                                input_.template read_data_array< int64_t >(
                                    data );
                        }
                    }
                    if( face_offsets_.size() != types.size() )
                    {
                        face_offsets_.clear();
                    }
                }

                // Faces stream of the cell, to be called for each cell in
                // order
                absl::Span< const int64_t > next_cell_faces( index_t cell )
                {
                    if( face_offsets_.empty() )
                    {
                        return {};
                    }
                    const auto end = face_offsets_[cell];
                    if( end < 0 )
                    {
                        return {};
                    }
                    const auto start = face_start_;
                    if( end < start
                        || end > static_cast< int64_t >( faces_.size() ) )
                    {
                        return {};
                    }
                    face_start_ = end;
                    return absl::MakeConstSpan( faces_ ).subspan(
                        static_cast< size_t >( start ),
                        static_cast< size_t >( end - start ) );
                }

                index_t create_polyhedron( absl::Span< const index_t > vertices,
                    absl::Span< const int64_t > faces )
                {
                    if( !input_.polyhedron_enabled_ )
                    {
                        nb_unsupported_polyhedra_++;
                        return NO_ID;
                    }
                    const auto facets = polyhedron_facets( vertices, faces );
                    if( !facets )
                    {
                        nb_invalid_polyhedra_++;
                        return NO_ID;
                    }
                    return input_.builder().create_polyhedron(
                        vertices, facets.value() );
                }

                void warn_skipped_polyhedra() const
                {
                    if( nb_unsupported_polyhedra_ > 0 )
                    {
                        Logger::warning( "[VTUInput] ",
                            nb_unsupported_polyhedra_,
                            " VTK_POLYHEDRON cell(s) cannot be loaded in a ",
                            Mesh::type_name_static().get(),
                            " and have been skipped" );
                    }
                    if( nb_invalid_polyhedra_ > 0 )
                    {
                        Logger::warning( "[VTUInput] ", nb_invalid_polyhedra_,
                            " VTK_POLYHEDRON cell(s) with invalid faces have "
                            "been skipped" );
                    }
                }

            private:
                // VTK faces are oriented outward, OpenGeode facets inward
                std::optional< std::vector< std::vector< local_index_t > > >
                    polyhedron_facets( absl::Span< const index_t > vertices,
                        absl::Span< const int64_t > faces ) const
                {
                    if( faces.empty() || vertices.size() >= NO_LID )
                    {
                        return std::nullopt;
                    }
                    const auto nb_faces = faces[0];
                    if( nb_faces < 4
                        || nb_faces > static_cast< int64_t >( faces.size() ) )
                    {
                        return std::nullopt;
                    }
                    std::vector< std::vector< local_index_t > > facets(
                        static_cast< size_t >( nb_faces ) );
                    size_t position{ 1 };
                    for( auto& facet : facets )
                    {
                        if( position >= faces.size() )
                        {
                            return std::nullopt;
                        }
                        const auto nb_face_vertices = faces[position++];
                        if( nb_face_vertices < 3
                            || nb_face_vertices > static_cast< int64_t >(
                                   faces.size() - position ) )
                        {
                            return std::nullopt;
                        }
                        facet.resize(
                            static_cast< size_t >( nb_face_vertices ) );
                        for( auto& facet_vertex : facet )
                        {
                            const auto vertex =
                                input_.piece_vertex( faces[position++] );
                            const auto it = absl::c_find( vertices, vertex );
                            if( it == vertices.end() )
                            {
                                return std::nullopt;
                            }
                            facet_vertex = static_cast< local_index_t >(
                                std::distance( vertices.begin(), it ) );
                        }
                        absl::c_reverse( facet );
                    }
                    return facets;
                }

            private:
                VTUSolidInput& input_;
                std::vector< int64_t > faces_;
                std::vector< int64_t > face_offsets_;
                int64_t face_start_{ 0 };
                index_t nb_unsupported_polyhedra_{ 0 };
                index_t nb_invalid_polyhedra_{ 0 };
            };

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
            static constexpr int64_t VTK_POLYHEDRON_TYPE{ 42 };
            absl::flat_hash_map< int64_t, VTKElement > elements_;
            bool polyhedron_enabled_{ false };
        };
    } // namespace detail
} // namespace geode
