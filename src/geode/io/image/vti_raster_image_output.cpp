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

#include <geode/io/image/detail/vti_raster_image_output.hpp>

#include <string>

#include <geode/image/core/raster_image.hpp>
#include <geode/image/core/rgb_color.hpp>

#include <geode/io/image/detail/vti_output_impl.hpp>

namespace
{
    template < geode::index_t dimension >
    class VTIOutputImpl
        : public geode::detail::VTIOutputImpl< geode::RasterImage< dimension > >
    {
    public:
        VTIOutputImpl( const geode::RasterImage< dimension >& raster,
            std::string_view filename )
            : geode::detail::VTIOutputImpl< geode::RasterImage< dimension > >{
                  raster, filename
              }
        {
        }

    private:
        void write_piece( pugi::xml_node& object ) final
        {
            auto piece = object.append_child( "Piece" );
            std::array< geode::index_t, dimension > extent;
            for( const auto d : geode::LRange{ dimension } )
            {
                extent[d] = this->mesh().nb_cells_in_direction( d );
            }
            this->write_image_header( piece, extent );
            write_point_data( piece );
        }

        void write_point_data( pugi::xml_node& piece )
        {
            auto point_data = piece.append_child( "PointData" );
            point_data.append_attribute( "Scalars" ).set_value( "Color" );
            std::vector< uint8_t > values;
            values.reserve( 3 * this->mesh().nb_cells() );
            for( const auto c : geode::Range{ this->mesh().nb_cells() } )
            {
                const auto& color = this->mesh().color( c );
                values.push_back( color.red() );
                values.push_back( color.green() );
                values.push_back( color.blue() );
            }
            this->template write_data_array< uint8_t >(
                point_data, "Color", values, 3 );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        std::vector< std::string > VTIRasterImageOutput< dimension >::write(
            const RasterImage< dimension >& raster ) const
        {
            ::VTIOutputImpl< dimension > impl{ raster, this->filename() };
            impl.write_file();
            return { to_string( this->filename() ) };
        }

        template class VTIRasterImageOutput< 2 >;
        template class VTIRasterImageOutput< 3 >;
    } // namespace detail
} // namespace geode
