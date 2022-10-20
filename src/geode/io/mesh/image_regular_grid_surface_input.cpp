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

#include <geode/io/mesh/private/image_regular_grid_surface_input.h>

#define cimg_display 0
#define cimg_use_jpeg
#define cimg_use_png
#include <cimg/CImg.h>

#include <geode/basic/attribute_manager.h>
#include <geode/basic/greyscale_color.h>
#include <geode/basic/rgb_color.h>

#include <geode/geometry/point.h>

#include <geode/mesh/builder/regular_grid_surface_builder.h>
#include <geode/mesh/core/regular_grid_surface.h>

namespace geode
{
    namespace detail
    {
        ImageInputImpl::ImageInputImpl(
            absl::string_view filename, geode::RegularGrid2D& grid )
            : filename_( filename ), grid_( grid )
        {
        }

        void ImageInputImpl::read_file()
        {
            const cimg_library::CImg< local_index_t > image( filename_.data() );
            auto builder = geode::RegularGridBuilder2D::create( grid_ );
            const std::array< geode::index_t, 2 > nb_cells{
                static_cast< geode::index_t >( image.width() ),
                static_cast< geode::index_t >( image.height() )
            };
            const auto nb_color_components =
                static_cast< geode::index_t >( image.spectrum() );
            builder->initialize_grid( { { 0, 0 } }, nb_cells, 1 );
            if( nb_color_components <= 2 )
            {
                auto image_attribute =
                    grid_.cell_attribute_manager()
                        .template find_or_create_attribute<
                            geode::VariableAttribute, geode::GreyscaleColor >(
                            "Greyscale_data", geode::GreyscaleColor() );
                for( const auto image_j : geode::Range{ nb_cells[1] } )
                {
                    const auto cell_j = nb_cells[1] - 1 - image_j;
                    for( const auto image_i : geode::Range{ nb_cells[0] } )
                    {
                        image_attribute->set_value(
                            grid_.cell_index( { image_i, cell_j } ),
                            { image( image_i, image_j ) } );
                    }
                }
            }
            else if( nb_color_components <= 4 )
            {
                auto image_attribute =
                    grid_.cell_attribute_manager()
                        .template find_or_create_attribute<
                            geode::VariableAttribute, geode::RGBColor >(
                            "RGB_data", geode::RGBColor() );
                for( const auto image_j : geode::Range{ nb_cells[1] } )
                {
                    const auto cell_j = nb_cells[1] - 1 - image_j;
                    for( const auto image_i : geode::Range{ nb_cells[0] } )
                    {
                        image_attribute->set_value(
                            grid_.cell_index( { image_i, cell_j } ),
                            { image( image_i, image_j, 0 ),
                                image( image_i, image_j, 0, 1 ),
                                image( image_i, image_j, 0, 2 ) } );
                    }
                }
            }
        }
    } // namespace detail
} // namespace geode