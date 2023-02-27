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

#include <geode/io/image/private/raster_image_input.h>

#define cimg_display 0
#define cimg_use_jpeg
#define cimg_use_png
#include <cimg/CImg.h>

#include <geode/basic/attribute_manager.h>
#include <geode/image/core/raster_image.h>
#include <geode/image/core/rgb_color.h>

namespace
{
    template < typename SpecificRange >
    geode::RasterImage2D read_file( absl::string_view filename )
    {
        const cimg_library::CImg< geode::local_index_t > image(
            geode::to_string( filename ).c_str() );
        geode::RasterImage2D raster{ { static_cast< geode::index_t >(
                                           image.width() ),
            static_cast< geode::index_t >( image.height() ) } };
        const auto nb_color_components =
            static_cast< geode::index_t >( image.spectrum() );
        if( nb_color_components <= 2 )
        {
            geode::index_t cell{ 0 };
            for( const auto image_j :
                SpecificRange{ raster.nb_cells_in_direction( 1 ) } )
            {
                for( const auto image_i :
                    geode::Range{ raster.nb_cells_in_direction( 0 ) } )
                {
                    const auto value = image( image_i, image_j );
                    raster.set_color( cell++, { value, value, value } );
                }
            }
        }
        else if( nb_color_components <= 4 )
        {
            geode::index_t cell{ 0 };
            for( const auto image_j :
                SpecificRange{ raster.nb_cells_in_direction( 1 ) } )
            {
                for( const auto image_i :
                    geode::Range{ raster.nb_cells_in_direction( 0 ) } )
                {
                    raster.set_color(
                        cell++, { image( image_i, image_j, 0 ),
                                    image( image_i, image_j, 0, 1 ),
                                    image( image_i, image_j, 0, 2 ) } );
                }
            }
        }
        return raster;
    }
} // namespace

namespace geode
{
    namespace detail
    {
        ImageInputImpl::ImageInputImpl( absl::string_view filename )
            : filename_( filename )
        {
        }

        RasterImage2D ImageInputImpl::read_file()
        {
            return ::read_file< Range >( filename_ );
        }

        RasterImage2D ImageInputImpl::read_reversed_y_axis_file()
        {
            return ::read_file< ReverseRange >( filename_ );
        }
    } // namespace detail
} // namespace geode