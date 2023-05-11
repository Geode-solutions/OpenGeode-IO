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

#include <geode/io/image/private/vti_raster_image_output.h>

#include <geode/image/core/raster_image.h>
#include <geode/image/core/rgb_color.h>

#include <geode/io/image/private/vti_output_impl.h>

namespace
{
    template < geode::index_t dim >
    class VTIOutputImpl
        : public geode::detail::VTIOutputImpl< geode::RasterImage< dim > >
    {
    public:
        VTIOutputImpl( const geode::RasterImage< dim >& raster,
            absl::string_view filename )
            : geode::detail::VTIOutputImpl< geode::RasterImage< dim > >{ raster,
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
                extent[d] = this->mesh().nb_cells_in_direction( d );
            }
            std::array< double, dim > spacing;
            spacing.fill( 1 );
            this->write_image_header( piece, {}, extent, spacing );
            write_point_data( piece );
        }

        void write_point_data( pugi::xml_node& piece )
        {
            auto point_data = piece.append_child( "PointData" );
            point_data.append_attribute( "Scalars" ).set_value( "Color" );
            auto data_array = point_data.append_child( "DataArray" );
            data_array.append_attribute( "type" ).set_value( "UInt8" );
            data_array.append_attribute( "Name" ).set_value( "Color" );
            data_array.append_attribute( "format" ).set_value( "ascii" );
            data_array.append_attribute( "NumberOfComponents" ).set_value( 3 );
            auto min = std::numeric_limits< geode::local_index_t >::max();
            auto max = std::numeric_limits< geode::local_index_t >::lowest();
            std::string values;
            for( const auto c : geode::Range{ this->mesh().nb_cells() } )
            {
                const auto& color = this->mesh().color( c );
                absl::StrAppend( &values, color.red(), " ", color.green(), " ",
                    color.blue(), " " );
                min = std::min(
                    min, std::min( color.red(),
                             std::min( color.green(), color.blue() ) ) );
                max = std::max(
                    max, std::max( color.red(),
                             std::max( color.green(), color.blue() ) ) );
            }
            data_array.append_attribute( "RangeMin" ).set_value( min );
            data_array.append_attribute( "RangeMax" ).set_value( max );
            data_array.text().set( values.c_str() );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        void VTIRasterImageOutput< dimension >::write(
            const RasterImage< dimension >& raster ) const
        {
            ::VTIOutputImpl< dimension > impl{ raster, this->filename() };
            impl.write_file();
        }

        template class VTIRasterImageOutput< 2 >;
        template class VTIRasterImageOutput< 3 >;
    } // namespace detail
} // namespace geode
