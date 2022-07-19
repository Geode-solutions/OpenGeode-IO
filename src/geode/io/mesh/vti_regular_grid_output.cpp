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

#include <geode/mesh/core/regular_grid_solid.h>
#include <geode/mesh/core/regular_grid_surface.h>

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
            write_vertex_data( piece );
            write_cell_data( piece );
        }

        void write_cell_data( pugi::xml_node& piece )
        {
            auto cell_data = piece.append_child( "CellData" );
            this->write_attributes(
                cell_data, this->mesh().cell_attribute_manager() );
        }

        void write_vertex_data( pugi::xml_node& piece )
        {
            auto vertex_data = piece.append_child( "PointData" );
            this->write_attributes(
                vertex_data, this->mesh().vertex_attribute_manager() );
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
                absl::StrAppend(
                    &extent, "0 ", this->mesh().nb_cells_in_direction( d ) );
            }
            if( dimension == 2 )
            {
                absl::StrAppend( &extent, " 0 0" );
            }
            image.append_attribute( "WholeExtent" ).set_value( extent.c_str() );
            piece.append_attribute( "Extent" ).set_value( extent.c_str() );
            std::string origin;
            absl::StrAppend( &origin, this->mesh().origin().string() );
            if( dimension == 2 )
            {
                absl::StrAppend( &origin, " 0" );
            }
            image.append_attribute( "Origin" ).set_value( origin.c_str() );
            std::string spacing;
            for( const auto d : geode::LRange{ dimension } )
            {
                if( d != 0 )
                {
                    absl::StrAppend( &spacing, " " );
                }
                absl::StrAppend(
                    &spacing, this->mesh().cell_length_in_direction( d ) );
            }
            if( dimension == 2 )
            {
                absl::StrAppend( &spacing, " 1" );
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
        void VTIRegularGridOutput< dimension >::write(
            const RegularGrid< dimension >& grid ) const
        {
            VTIOutputImpl< dimension > impl{ grid, this->filename() };
            impl.write_file();
        }

        template class opengeode_io_mesh_api VTIRegularGridOutput< 2 >;
        template class opengeode_io_mesh_api VTIRegularGridOutput< 3 >;
    } // namespace detail
} // namespace geode
