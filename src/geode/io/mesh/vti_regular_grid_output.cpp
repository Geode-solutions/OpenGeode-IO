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

#include <geode/io/mesh/private/vti_regular_grid_output.h>

#include <geode/mesh/core/regular_grid_solid.h>
#include <geode/mesh/core/regular_grid_surface.h>

#include <geode/io/image/private/vti_output_impl.h>

namespace
{
    template < geode::index_t dim >
    class VTIOutputImpl
        : public geode::detail::VTIOutputImpl< geode::RegularGrid< dim > >
    {
    public:
        VTIOutputImpl(
            const geode::RegularGrid< dim >& grid, absl::string_view filename )
            : geode::detail::VTIOutputImpl< geode::RegularGrid< dim > >{ grid,
                  filename }
        {
        }

    private:
        void write_piece( pugi::xml_node& object ) final
        {
            auto piece = object.append_child( "Piece" );
            std::array< geode::index_t, dim > extent;
            for( const auto d : geode::LRange{ dim } )
            {
                extent[d] = this->mesh().nb_vertices_in_direction( d );
            }
            std::array< double, dim > spacing;
            for( const auto d : geode::LRange{ dim } )
            {
                spacing[d] = this->mesh().cell_length_in_direction( d );
            }
            this->write_image_header(
                piece, this->mesh().origin(), extent, spacing );
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
            ::VTIOutputImpl< dimension > impl{ grid, this->filename() };
            impl.write_file();
        }

        template class VTIRegularGridOutput< 2 >;
        template class VTIRegularGridOutput< 3 >;
    } // namespace detail
} // namespace geode
