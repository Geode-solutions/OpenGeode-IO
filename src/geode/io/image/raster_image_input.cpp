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

#include <cmath>
#include <limits>
#include <optional>
#include <vector>

#include <gdal_priv.h>

#include <absl/algorithm/container.h>
#include <absl/types/span.h>

#include <geode/basic/logger.hpp>
#include <geode/basic/percentage.hpp>

#include <geode/image/core/detail/color_channel.hpp>
#include <geode/image/core/raster_image.hpp>
#include <geode/image/core/rgb_color.hpp>

namespace
{
    // Image formats only: other GDAL formats (e.g. VRT) can reference any
    // other file or URL
    constexpr std::array< const char*, 4 > IMAGE_DRIVERS{ "PNG", "JPEG", "BMP",
        "GTiff" };

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

    // Colors of the palette entries when the band is a palette index
    std::optional< std::vector< geode::RGBColor > > palette_colors(
        GDALRasterBand& band )
    {
        if( band.GetColorInterpretation() != GCI_PaletteIndex )
        {
            return std::nullopt;
        }
        const auto* table = band.GetColorTable();
        if( table == nullptr )
        {
            return std::nullopt;
        }
        const auto interpretation = table->GetPaletteInterpretation();
        if( interpretation != GPI_RGB && interpretation != GPI_Gray )
        {
            geode::Logger::warning( "[ImageInputImpl] Palette is neither RGB "
                                    "nor grey: palette indices are read as "
                                    "grey levels" );
            return std::nullopt;
        }
        std::vector< geode::RGBColor > colors(
            static_cast< size_t >( table->GetColorEntryCount() ) );
        for( auto entry_id = 0; entry_id < table->GetColorEntryCount();
            entry_id++ )
        {
            const auto& entry = *table->GetColorEntry( entry_id );
            const auto red = geode::detail::color_channel( entry.c1 );
            if( interpretation == GPI_Gray )
            {
                colors[entry_id] = { red, red, red };
                continue;
            }
            colors[entry_id] = { red, geode::detail::color_channel( entry.c2 ),
                geode::detail::color_channel( entry.c3 ) };
        }
        return colors;
    }

    void read_palette_band( GDALRasterBand& band,
        absl::Span< const geode::RGBColor > palette,
        absl::Span< geode::RGBColor > colors,
        int width,
        int height )
    {
        std::vector< GUInt16 > indices( colors.size() );
        constexpr auto INDEX_SIZE =
            static_cast< GSpacing >( sizeof( GUInt16 ) );
        check_read( band.RasterIO( GF_Read, 0, 0, width, height,
            indices.data() + indices.size() - width, width, height, GDT_UInt16,
            INDEX_SIZE, -INDEX_SIZE * width ) );
        for( const auto pixel : geode::Indices{ indices } )
        {
            const auto index = indices[pixel];
            // Indices without palette entry are black
            colors[pixel] =
                index < palette.size() ? palette[index] : geode::RGBColor{};
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

    bool are_byte_bands( GDALDataset& dataset, absl::Span< const int > bands )
    {
        return absl::c_all_of( bands, [&dataset]( int band ) {
            return dataset.GetRasterBand( band )->GetRasterDataType()
                   == GDT_Byte;
        } );
    }

    struct ValueRange
    {
        double min{ std::numeric_limits< double >::max() };
        double max{ std::numeric_limits< double >::lowest() };
    };

    // Range of the band values, nodata values excluded
    ValueRange bands_range(
        GDALDataset& dataset, absl::Span< const int > bands )
    {
        ValueRange range;
        for( const auto band : bands )
        {
            std::array< double, 2 > min_max;
            if( dataset.GetRasterBand( band )->ComputeRasterMinMax(
                    false, min_max.data() )
                == CE_None )
            {
                range.min = std::min( range.min, min_max[0] );
                range.max = std::max( range.max, min_max[1] );
            }
        }
        return range;
    }

    void set_channel(
        geode::RGBColor& color, size_t channel, geode::local_index_t value )
    {
        if( channel == 0 )
        {
            color.set_red( value );
        }
        else if( channel == 1 )
        {
            color.set_green( value );
        }
        else
        {
            color.set_blue( value );
        }
    }

    // Values which are not 8-bit are linearly rescaled from the range of the
    // values of all the bands to the color range, nodata values are black
    void read_rescaled_bands( GDALDataset& dataset,
        std::array< int, 3 > bands,
        absl::Span< geode::RGBColor > colors,
        int width,
        int height )
    {
        const auto range = bands_range( dataset, bands );
        const auto scale =
            range.max > range.min
                ? std::numeric_limits< geode::local_index_t >::max()
                      / ( range.max - range.min )
                : 0.;
        std::vector< double > values( colors.size() );
        constexpr auto VALUE_SIZE = static_cast< GSpacing >( sizeof( double ) );
        for( const auto channel : geode::Indices{ bands } )
        {
            if( absl::c_find( bands, bands[channel] )
                != bands.begin() + channel )
            {
                // Band already read for a previous channel
                continue;
            }
            auto& band = *dataset.GetRasterBand( bands[channel] );
            check_read( band.RasterIO( GF_Read, 0, 0, width, height,
                values.data() + values.size() - width, width, height,
                GDT_Float64, VALUE_SIZE, -VALUE_SIZE * width ) );
            int has_nodata{ 0 };
            const auto nodata = band.GetNoDataValue( &has_nodata );
            for( const auto pixel : geode::Indices{ values } )
            {
                const auto value = values[pixel];
                const auto is_nodata =
                    std::isnan( value ) || ( has_nodata && value == nodata );
                const auto channel_value =
                    is_nodata ? geode::local_index_t{ 0 }
                              : geode::detail::color_channel(
                                    ( value - range.min ) * scale );
                for( const auto other : geode::Indices{ bands } )
                {
                    if( bands[other] == bands[channel] )
                    {
                        set_channel( colors[pixel], other, channel_value );
                    }
                }
            }
        }
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
            : detail::GDALFile{ filename, IMAGE_DRIVERS }
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
            auto& first_band = *gdal_data.GetRasterBand( 1 );
            if( const auto palette = palette_colors( first_band ) )
            {
                read_palette_band(
                    first_band, palette.value(), colors, width, height );
            }
            else
            {
                const auto bands = nb_color_components <= 2
                                       ? std::array< int, 3 >{ 1, 1, 1 }
                                       : get_rgb_indices( gdal_data );
                if( !are_byte_bands( gdal_data, bands ) )
                {
                    read_rescaled_bands(
                        gdal_data, bands, colors, width, height );
                }
                else if( nb_color_components <= 2 )
                {
                    read_grey_band( first_band, colors, width, height );
                }
                else
                {
                    read_rgb_bands( gdal_data, bands, colors, width, height );
                }
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
