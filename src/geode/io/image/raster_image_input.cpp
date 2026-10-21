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

#include <geode/io/image/internal/raster_image_input.hpp>

#include <gdal_priv.h>

#include <absl/algorithm/container.h>
#include <absl/types/span.h>

#include <geode/basic/logger.hpp>
#include <geode/basic/percentage.hpp>

#include <geode/image/core/raster_image.hpp>
#include <geode/image/core/rgb_color.hpp>

namespace
{
    static_assert( sizeof( geode::RGBColor ) == 3,
        "[ImageInputImpl] Colors should be stored as 3 bytes" );

    // Image files store their top row first whereas raster images start from
    // the bottom row: GDAL writes the rows backward from the last one
    GByte* last_row_start( absl::Span< geode::RGBColor > colors, int width )
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast< GByte* >( colors.data() + colors.size() )
               - static_cast< std::ptrdiff_t >( 3 ) * width;
    }

    void check_read( CPLErr status )
    {
        geode::OpenGeodeIOImageException::check_exception( status == CE_None,
            nullptr, geode::OpenGeodeException::TYPE::data,
            "[ImageInputImpl] Failed to read image pixels" );
    }

    void read_grey_band( GDALRasterBand& band,
        absl::Span< geode::RGBColor > colors,
        int width,
        int height )
    {
        check_read( band.RasterIO( GF_Read, 0, 0, width, height,
            last_row_start( colors, width ), width, height, GDT_Byte, 3,
            -static_cast< GSpacing >( 3 ) * width ) );
        for( auto& color : colors )
        {
            color.set_green( color.red() );
            color.set_blue( color.red() );
        }
    }

    void read_rgb_bands( GDALDataset& dataset,
        std::array< int, 3 > bands,
        absl::Span< geode::RGBColor > colors,
        int width,
        int height )
    {
        check_read( dataset.RasterIO( GF_Read, 0, 0, width, height,
            last_row_start( colors, width ), width, height, GDT_Byte, 3,
            bands.data(), 3, -static_cast< GSpacing >( 3 ) * width, 1,
            nullptr ) );
    }

    std::array< int, 3 > get_rgb_indices( GDALDataset& gdal_data )
    {
        std::array< int, 3 > rgb_indices{ 0, 0, 0 };
        const auto nb_color_components = gdal_data.GetRasterCount();
        for( int band_id = 1; band_id <= nb_color_components; band_id++ )
        {
            GDALColorInterp colorInterp =
                gdal_data.GetRasterBand( band_id )->GetColorInterpretation();
            if( colorInterp == GCI_RedBand )
            {
                rgb_indices[0] = band_id;
            }
            else if( colorInterp == GCI_GreenBand )
            {
                rgb_indices[1] = band_id;
            }
            else if( colorInterp == GCI_BlueBand )
            {
                rgb_indices[2] = band_id;
            }
        }
        if( absl::c_linear_search( rgb_indices, 0 ) )
        {
            geode::Logger::warning( "[ImageInputImpl] Red, green and blue "
                                    "bands are not identified: the first "
                                    "three bands are used" );
            return { 1, 2, 3 };
        }
        return rgb_indices;
    }
} // namespace

namespace geode
{
    namespace internal
    {
        ImageInputImpl::ImageInputImpl( std::string_view filename )
            : detail::GDALFile{ filename }
        {
        }

        RasterImage2D ImageInputImpl::read_file()
        {
            auto& gdal_data = dataset();
            const auto width = gdal_data.GetRasterXSize();
            const auto height = gdal_data.GetRasterYSize();
            RasterImage2D raster{ { static_cast< index_t >( width ),
                static_cast< index_t >( height ) } };
            const auto colors = raster.modifiable_colors();
            const auto nb_color_components = gdal_data.GetRasterCount();
            if( nb_color_components <= 2 )
            {
                read_grey_band(
                    *gdal_data.GetRasterBand( 1 ), colors, width, height );
            }
            else
            {
                read_rgb_bands( gdal_data, get_rgb_indices( gdal_data ), colors,
                    width, height );
            }
            return raster;
        }

        Percentage ImageInputImpl::is_loadable()
        {
            if( dataset().GetRasterCount() == 0 )
            {
                return Percentage{ 0 };
            }
            return Percentage{ 1 };
        }
    } // namespace internal
} // namespace geode
