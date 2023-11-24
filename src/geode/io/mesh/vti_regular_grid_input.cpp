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

#include <geode/io/mesh/private/vti_regular_grid_input.h>

#include <array>
#include <fstream>

#include <geode/basic/string.h>

#include <geode/geometry/coordinate_system.h>

#include <geode/mesh/builder/regular_grid_solid_builder.h>
#include <geode/mesh/builder/regular_grid_surface_builder.h>
#include <geode/mesh/core/regular_grid_solid.h>
#include <geode/mesh/core/regular_grid_surface.h>

#include <geode/io/mesh/private/vtk_input.h>

namespace
{
    template < geode::index_t dimension >
    struct GridAttributes
    {
        GridAttributes()
        {
            cells_number.fill( 0 );
            cells_length.fill( 1 );
            for( const auto d : geode::LRange{ dimension } )
            {
                cell_directions[d].set_value( d, 1 );
            }
        }

        geode::Point< dimension > origin;
        std::array< geode::index_t, dimension > cells_number;
        std::array< double, dimension > cells_length;
        std::array< geode::Vector< dimension >, dimension > cell_directions;
    };

    template < geode::index_t dimension >
    GridAttributes< dimension > read_grid_attributes(
        const pugi::xml_node& vtk_object )
    {
        GridAttributes< dimension > grid;
        for( const auto& attribute : vtk_object.attributes() )
        {
            if( std::strcmp( attribute.name(), "WholeExtent" ) == 0 )
            {
                const auto tokens = geode::string_split( attribute.value() );
                for( const auto d : geode::LRange{ dimension } )
                {
                    const auto start = geode::string_to_index( tokens[2 * d] );
                    const auto end =
                        geode::string_to_index( tokens[2 * d + 1] );
                    grid.cells_number[d] = end - start;
                }
            }
            else if( std::strcmp( attribute.name(), "Origin" ) == 0 )
            {
                const auto tokens = geode::string_split( attribute.value() );
                for( const auto d : geode::LRange{ dimension } )
                {
                    grid.origin.set_value(
                        d, geode::string_to_double( tokens[d] ) );
                }
            }
            else if( std::strcmp( attribute.name(), "Spacing" ) == 0 )
            {
                const auto tokens = geode::string_split( attribute.value() );
                for( const auto d : geode::LRange{ dimension } )
                {
                    grid.cells_length[d] = geode::string_to_double( tokens[d] );
                }
            }
            else if( std::strcmp( attribute.name(), "Direction" ) == 0 )
            {
                const auto tokens = geode::string_split( attribute.value() );
                for( const auto d : geode::LRange{ dimension } )
                {
                    auto& direction = grid.cell_directions[d];
                    for( const auto i : geode::LRange{ dimension } )
                    {
                        direction.set_value(
                            i, geode::string_to_double( tokens[3 * d + i] ) );
                    }
                }
            }
        }
        for( const auto d : geode::LRange{ dimension } )
        {
            grid.cell_directions[d] *= grid.cells_length[d];
        }
        return grid;
    }

    template < geode::index_t dimension >
    class VTIRegularGridInputImpl
        : public geode::detail::VTKInputImpl< geode::RegularGrid< dimension > >
    {
    public:
        VTIRegularGridInputImpl(
            absl::string_view filename, geode::RegularGrid< dimension >& grid )
            : geode::detail::VTKInputImpl< geode::RegularGrid< dimension > >{
                  filename, grid, "ImageData"
              }
        {
        }

    private:
        void read_vtk_object( const pugi::xml_node& vtk_object ) final
        {
            build_grid( vtk_object );
            for( const auto& piece : vtk_object.children( "Piece" ) )
            {
                this->read_point_data( piece.child( "PointData" ), 0 );
                read_cell_data( piece.child( "CellData" ) );
            }
        }

        void build_grid( const pugi::xml_node& vtk_object )
        {
            const auto grid_attributes =
                read_grid_attributes< dimension >( vtk_object );
            this->builder().initialize_grid( grid_attributes.origin,
                grid_attributes.cells_number, grid_attributes.cell_directions );
        }

        void read_cell_data( const pugi::xml_node& point_data )
        {
            for( const auto& data : point_data.children( "DataArray" ) )
            {
                this->read_attribute_data(
                    data, 0, this->mesh().cell_attribute_manager() );
            }
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        std::unique_ptr< RegularGrid< dimension > >
            VTIRegularGridInput< dimension >::read( const MeshImpl& impl )
        {
            auto grid = RegularGrid< dimension >::create( impl );
            VTIRegularGridInputImpl< dimension > reader{ this->filename(),
                *grid };
            reader.read_file();
            return grid;
        }

        template < index_t dimension >
        bool VTIRegularGridInput< dimension >::is_loadable() const
        {
            std::ifstream file{ to_string( this->filename() ) };
            OPENGEODE_EXCEPTION( file.good(),
                "[VTIRegularGridInput::is_loadable] Error while opening file: ",
                this->filename() );
            pugi::xml_document document;
            const auto status =
                document.load_file( to_string( this->filename() ).c_str() );
            OPENGEODE_EXCEPTION( status,
                "[VTIRegularGridInput::is_loadable] Error ",
                status.description(),
                " while parsing file: ", this->filename() );
            const auto node = document.child( "VTKFile" ).child( "ImageData" );
            const auto grid_attributes =
                read_grid_attributes< dimension >( node );
            const auto nb_cells_3d = grid_attributes.cells_number[2];
            return dimension == 2 ? nb_cells_3d == 0 : nb_cells_3d > 0;
        }

        template class VTIRegularGridInput< 2 >;
        template class VTIRegularGridInput< 3 >;
    } // namespace detail
} // namespace geode
