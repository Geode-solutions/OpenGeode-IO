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

#include <geode/io/mesh/detail/vtk_mesh_input.hpp>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class VTUInputImpl : public VTKMeshInputImpl< Mesh >
        {
        protected:
            VTUInputImpl(
                std::string_view filename, const geode::MeshImpl& impl )
                : VTKMeshInputImpl< Mesh >( filename, impl, "UnstructuredGrid" )
            {
            }

            std::tuple< absl::FixedArray< std::vector< index_t > >,
                std::vector< uint8_t > >
                read_cells(
                    const pugi::xml_node& piece, index_t nb_cells ) const
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
                        OPENGEODE_EXCEPTION(
                            this->match(
                                data.attribute( "type" ).value(), "Int64" ),
                            "[VTUInputImpl::read_cells] Wrong offset type, "
                            "supports only Int64" );
                        offsets_values =
                            this->template read_integer_data_array< int64_t >(
                                data );
                        OPENGEODE_ASSERT( offsets_values.size() == nb_cells,
                            "[VTUInputImpl::read_cells] Wrong number of "
                            "offsets" );
                        geode_unused( nb_cells );
                    }
                    else if( this->match( data.attribute( "Name" ).value(),
                                 "connectivity" ) )
                    {
                        OPENGEODE_EXCEPTION(
                            this->match(
                                data.attribute( "type" ).value(), "Int64" ),
                            "[VTUInputImpl::read_cells] Wrong connectivity "
                            "type, supports only Int64" );
                        connectivity_values =
                            this->template read_integer_data_array< int64_t >(
                                data );
                    }
                    else if( this->match(
                                 data.attribute( "Name" ).value(), "types" ) )
                    {
                        if( this->match(
                                data.attribute( "type" ).value(), "UInt8" ) )
                        {
                            types_values =
                                this->template read_uint8_data_array< uint8_t >(
                                    data );
                        }
                        else if( this->match( data.attribute( "type" ).value(),
                                     "Int32" ) )
                        {
                            types_values.reserve( nb_cells );
                            for( const auto type :
                                this->template read_integer_data_array<
                                    int32_t >( data ) )
                            {
                                types_values.push_back(
                                    static_cast< uint8_t >( type ) );
                            }
                        }
                        else
                        {
                            throw OpenGeodeException(
                                "[VTUInputImpl::read_cells] Wrong types type" );
                        }
                        OPENGEODE_ASSERT( types_values.size() == nb_cells,
                            "[VTUInputImpl::read_cells] Wrong number of "
                            "types" );
                        geode_unused( nb_cells );
                    }
                }
                return std::tuple{ this->get_cell_vertices(
                                       connectivity_values, offsets_values ),
                    types_values };
            }
        };
    } // namespace detail
} // namespace geode
