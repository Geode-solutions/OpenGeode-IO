/*
 * Copyright (c) 2019 - 2020 Geode-solutions
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

#include <geode/io/mesh/private/vtu_output.h>

#include <geode/mesh/core/tetrahedral_solid.h>

#include <geode/io/mesh/private/vtk_output.h>

namespace
{
    class VTUOutputImpl
        : public geode::detail::VTKOutputImpl< geode::TetrahedralSolid3D >
    {
    public:
        VTUOutputImpl( absl::string_view filename,
            const geode::TetrahedralSolid3D& tetrahedral_solid )
            : geode::detail::VTKOutputImpl< geode::TetrahedralSolid3D >(
                filename, tetrahedral_solid, "UnstructuredGrid" )
        {
        }

    private:
        void append_number_elements( pugi::xml_node& piece ) override
        {
            piece.append_attribute( "NumberOfCells" )
                .set_value( mesh().nb_polyhedra() );
        }

        void write_vtk_cells( pugi::xml_node& piece ) override
        {
            auto cells = piece.append_child( "Cells" );
            auto connectivity = cells.append_child( "DataArray" );
            connectivity.append_attribute( "type" ).set_value( "Int64" );
            connectivity.append_attribute( "Name" ).set_value( "connectivity" );
            connectivity.append_attribute( "format" ).set_value( "ascii" );
            connectivity.append_attribute( "RangeMin" ).set_value( 0 );
            connectivity.append_attribute( "RangeMax" )
                .set_value( mesh().nb_vertices() - 1 );
            auto offsets = cells.append_child( "DataArray" );
            offsets.append_attribute( "type" ).set_value( "Int64" );
            offsets.append_attribute( "Name" ).set_value( "offsets" );
            offsets.append_attribute( "format" ).set_value( "ascii" );
            offsets.append_attribute( "RangeMin" ).set_value( 0 );
            offsets.append_attribute( "RangeMax" )
                .set_value( mesh().nb_vertices() );
            auto types = cells.append_child( "DataArray" );
            types.append_attribute( "type" ).set_value( "UInt8" );
            types.append_attribute( "Name" ).set_value( "types" );
            types.append_attribute( "format" ).set_value( "ascii" );
            types.append_attribute( "RangeMin" ).set_value( 1 );
            types.append_attribute( "RangeMax" ).set_value( 14 );
            const auto nb_cells = mesh().nb_polyhedra();
            std::string cell_connectivity;
            cell_connectivity.reserve( nb_cells * 3 * 4 );
            std::string cell_offsets;
            cell_offsets.reserve( nb_cells );
            std::string cell_types;
            cell_types.reserve( nb_cells );
            geode::index_t offset{ 0 };
            for( const auto p : geode::Range{ nb_cells } )
            {
                const auto nb_vertices = mesh().nb_polyhedron_vertices( p );
                OPENGEODE_EXCEPTION( nb_vertices == 4,
                    "[VTUOutput::write_vtk_cells] Only tetrahedron elements "
                    "are implemented yet" );
                offset += 4 * 3;
                absl::StrAppend( &cell_offsets, offset, " " );
                absl::StrAppend( &cell_types, 10, " " );
                for( const auto f : geode::Range{ 4 } )
                {
                    const geode::PolyhedronFacet facet{ p, f };
                    for( const auto v : geode::Range{ 3 } )
                    {
                        absl::StrAppend( &cell_connectivity,
                            mesh().polyhedron_facet_vertex( { facet, v } ),
                            " " );
                    }
                }
            }
            connectivity.text().set( cell_connectivity.c_str() );
            offsets.text().set( cell_offsets.c_str() );
            types.text().set( cell_types.c_str() );
        }

        void write_vtk_cell_attributes( pugi::xml_node& piece ) override
        {
            auto cell_data = piece.append_child( "CellData" );
            const auto names =
                mesh().polyhedron_attribute_manager().attribute_names();
            for( const auto name : names )
            {
                const auto attribute = mesh()
                                           .polyhedron_attribute_manager()
                                           .find_generic_attribute( name );
                if( !attribute || !attribute->is_genericable() )
                {
                    continue;
                }
                auto data_array = cell_data.append_child( "DataArray" );
                data_array.append_attribute( "type" ).set_value( "Float64" );
                data_array.append_attribute( "Name" ).set_value( name.data() );
                data_array.append_attribute( "format" ).set_value( "ascii" );
                auto min = attribute->generic_value( 0 );
                auto max = attribute->generic_value( 0 );
                std::string values;
                for( const auto v : geode::Range{ mesh().nb_polyhedra() } )
                {
                    const auto value = attribute->generic_value( v );
                    absl::StrAppend( &values, value, " " );
                    min = std::min( min, value );
                    max = std::max( max, value );
                }
                data_array.append_attribute( "RangeMin" ).set_value( min );
                data_array.append_attribute( "RangeMax" ).set_value( max );
                data_array.text().set( values.c_str() );
            }
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void VTUOutput::write() const
        {
            VTUOutputImpl impl{ filename(), tetrahedral_solid() };
            impl.write_file();
        }
    } // namespace detail
} // namespace geode
