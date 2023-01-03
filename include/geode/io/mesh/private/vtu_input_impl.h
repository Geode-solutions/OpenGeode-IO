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

#include <geode/io/mesh/private/vtk_input.h>

namespace geode
{
    namespace detail
    {
        template < typename Mesh, typename Builder >
        class VTUInputImpl : public VTKInputImpl< Mesh, Builder >
        {
            using VTKElement = absl::FixedArray< std::vector< local_index_t > >;

        protected:
            VTUInputImpl( absl::string_view filename, Mesh& solid )
                : VTKInputImpl< Mesh, Builder >(
                    filename, solid, "UnstructuredGrid" ),
                  vtk_tetrahedron_( 4 ),
                  vtk_hexahedron_( 6 ),
                  vtk_prism_( 5 ),
                  vtk_pyramid_( 5 )
            {
                vtk_tetrahedron_[0] = { 1, 3, 2 };
                vtk_tetrahedron_[1] = { 0, 2, 3 };
                vtk_tetrahedron_[2] = { 3, 1, 0 };
                vtk_tetrahedron_[3] = { 0, 1, 2 };
                vtk_hexahedron_[0] = { 0, 4, 5, 1 };
                vtk_hexahedron_[1] = { 1, 5, 7, 3 };
                vtk_hexahedron_[2] = { 3, 7, 6, 2 };
                vtk_hexahedron_[3] = { 2, 6, 4, 0 };
                vtk_hexahedron_[4] = { 4, 6, 7, 5 };
                vtk_hexahedron_[5] = { 0, 1, 3, 2 };
                vtk_prism_[0] = { 0, 2, 1 };
                vtk_prism_[1] = { 3, 4, 5 };
                vtk_prism_[2] = { 0, 3, 5, 2 };
                vtk_prism_[3] = { 1, 2, 5, 4 };
                vtk_prism_[4] = { 0, 1, 4, 3 };
                vtk_pyramid_[0] = { 0, 4, 1 };
                vtk_pyramid_[1] = { 1, 4, 2 };
                vtk_pyramid_[2] = { 2, 4, 3 };
                vtk_pyramid_[3] = { 3, 4, 0 };
                vtk_pyramid_[4] = { 0, 1, 2, 3 };
            }

            void enable_tetrahedron()
            {
                elements_.emplace( 10, vtk_tetrahedron_ );
            }

            void enable_hexahedron()
            {
                elements_.emplace( 11, vtk_hexahedron_ ); // voxel
                elements_.emplace( 12, vtk_hexahedron_ ); // hexahedron
            }

            void enable_prism()
            {
                elements_.emplace( 13, vtk_prism_ );
            }

            void enable_pyramid()
            {
                elements_.emplace( 14, vtk_pyramid_ );
            }

        private:
            void read_vtk_cells( const pugi::xml_node& piece ) override
            {
                const auto nb_polyhedra =
                    this->read_attribute( piece, "NumberOfCells" );
                const auto polyhedron_offset =
                    build_polyhedra( piece, nb_polyhedra );
                this->builder().compute_polyhedron_adjacencies();
                read_cell_data( piece.child( "CellData" ), polyhedron_offset );
            }

            std::tuple< absl::FixedArray< std::vector< index_t > >,
                std::vector< uint8_t > >
                read_polyhedra(
                    const pugi::xml_node& piece, index_t nb_polyhedra )
            {
                std::vector< int64_t > offsets_values;
                std::vector< int64_t > connectivity_values;
                std::vector< uint8_t > types_values;
                for( const auto& data :
                    piece.child( "Cells" ).children( "DataArray" ) )
                {
                    if( this->match(
                            data.attribute( "Name" ).value(), "offsets" ) )
                    {
                        offsets_values =
                            this->template read_integer_data_array< int64_t >(
                                data );
                        OPENGEODE_ASSERT( offsets_values.size() == nb_polyhedra,
                            "[VTUInput::read_polyhedra] Wrong number of "
                            "offsets" );
                        geode_unused( nb_polyhedra );
                    }
                    else if( this->match( data.attribute( "Name" ).value(),
                                 "connectivity" ) )
                    {
                        connectivity_values =
                            this->template read_integer_data_array< int64_t >(
                                data );
                    }
                    else if( this->match(
                                 data.attribute( "Name" ).value(), "types" ) )
                    {
                        types_values =
                            this->template read_uint8_data_array< uint8_t >(
                                data );
                        OPENGEODE_ASSERT( types_values.size() == nb_polyhedra,
                            "[VTUInput::read_polyhedra] Wrong number of "
                            "types" );
                        geode_unused( nb_polyhedra );
                    }
                }
                return std::make_tuple(
                    this->get_cell_vertices(
                        connectivity_values, offsets_values ),
                    types_values );
            }

            index_t build_polyhedra(
                const pugi::xml_node& piece, index_t nb_polyhedra )
            {
                const auto output = read_polyhedra( piece, nb_polyhedra );
                const auto& polyhedron_vertices = std::get< 0 >( output );
                const auto& types = std::get< 1 >( output );
                const auto polyhedra_offset = this->mesh().nb_polyhedra();
                for( const auto p : Indices{ polyhedron_vertices } )
                {
                    const auto it = elements_.find( types[p] );
                    if( it != elements_.end() )
                    {
                        this->builder().create_polyhedron(
                            polyhedron_vertices[p], it->second );
                    }
                }
                return polyhedra_offset;
            }

            void read_cell_data(
                const pugi::xml_node& point_data, index_t offset )
            {
                for( const auto& data : point_data.children( "DataArray" ) )
                {
                    this->read_attribute_data( data, offset,
                        this->mesh().polyhedron_attribute_manager() );
                }
            }

        private:
            absl::flat_hash_map< int64_t, VTKElement > elements_;
            VTKElement vtk_tetrahedron_;
            VTKElement vtk_hexahedron_;
            VTKElement vtk_prism_;
            VTKElement vtk_pyramid_;
        };
    } // namespace detail
} // namespace geode
