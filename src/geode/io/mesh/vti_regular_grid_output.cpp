/*
 * Copyright (c) 2019 - 2022 Geode-solutions
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

#include <geode/io/mesh/private/vti_regular_grid_output.h>

#include <geode/geometry/bounding_box.h>
#include <geode/geometry/point.h>

#include <geode/mesh/core/regular_grid.h>

#include <geode/io/mesh/private/vtk_output.h>

namespace
{
    template < geode::index_t dimension >
    class VTIOutputImpl
        : public geode::detail::VTKOutputImpl< geode::RegularGrid< dimension > >
    {
    public:
        VTIOutputImpl( const geode::RegularGrid< dimension >& grid,
            absl::string_view filename )
            : geode::detail::VTKOutputImpl< geode::RegularGrid< dimension > >{
                  filename, grid, "ImageData"
              }
        {
        }

    private:
        void write_piece( pugi::xml_node& piece ) final
        {
            write_image_header( piece );
            write_cell_data( piece );
        }

        void write_cell_data( pugi::xml_node& piece )
        {
            auto cell_data = piece.append_child( "CellData" );
            const auto names =
                this->mesh().cell_attribute_manager().attribute_names();
            for( const auto& name : names )
            {
                const auto attribute = this->mesh()
                                           .cell_attribute_manager()
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
                for( const auto c : geode::Range{ this->mesh().nb_cells() } )
                {
                    const auto value = attribute->generic_value( c );
                    absl::StrAppend( &values, value, " " );
                    min = std::min( min, value );
                    max = std::max( max, value );
                }
                data_array.append_attribute( "RangeMin" ).set_value( min );
                data_array.append_attribute( "RangeMax" ).set_value( max );
                data_array.text().set( values.c_str() );
            }
        }

        void write_image_header( pugi::xml_node& piece )
        {
            auto image = piece.parent();
            std::string extent;
            for( const auto d : geode::LRange{ dimension } )
            {
                if( d != 0 )
                {
                    absl::StrAppend( &extent, " " );
                }
                absl::StrAppend( &extent, "0 ", this->mesh().nb_cells( d ) );
            }
            image.append_attribute( "WholeExtent" ).set_value( extent.c_str() );
            piece.append_attribute( "Extent" ).set_value( extent.c_str() );
            image.append_attribute( "Origin" )
                .set_value( this->mesh().origin().string().c_str() );
            std::string spacing;
            for( const auto d : geode::LRange{ dimension } )
            {
                if( d != 0 )
                {
                    absl::StrAppend( &spacing, " " );
                }
                absl::StrAppend( &spacing, this->mesh().cell_size( d ) );
            }
            image.append_attribute( "Spacing" ).set_value( spacing.c_str() );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        void VTIRegularGridOutput< dimension >::write() const
        {
            VTIOutputImpl< dimension > impl{ this->regular_grid(),
                this->filename() };
            impl.write_file();
        }

        template class opengeode_io_mesh_api VTIRegularGridOutput< 2 >;
        template class opengeode_io_mesh_api VTIRegularGridOutput< 3 >;
    } // namespace detail
} // namespace geode
