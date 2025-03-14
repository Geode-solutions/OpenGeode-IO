/*
 * Copyright (c) 2019 - 2025 Geode-solutions
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

#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/logger.hpp>

#include <geode/image/core/raster_image.hpp>
#include <geode/image/core/rgb_color.hpp>

namespace
{
    absl::FixedArray< GByte > read_color_component(
        const geode::RasterImage2D& raster,
        GDALDatasetUniquePtr& gdal_data,
        int component )
    {
        const auto width = raster.nb_cells_in_direction( 0 );
        const auto height = raster.nb_cells_in_direction( 1 );
        absl::FixedArray< GByte > values( raster.nb_cells() );
        const auto status =
            gdal_data->GetRasterBand( component )
                ->RasterIO( GF_Read, 0, 0, width, height, values.data(), width,
                    height, GDT_Byte, 0, 0 );
        OPENGEODE_EXCEPTION( status == CE_None,
            "[ImageInputImpl] Failed to read color component" );
        return values;
    }
    std::array< geode::index_t, 3 > get_rgb_indices(
        GDALDatasetUniquePtr& gdal_data )
    {
        std::array< geode::index_t, 3 > rgb_indices{ 0, 0, 0 };
        const auto nb_color_components =
            static_cast< geode::index_t >( gdal_data->GetRasterCount() );
        for( const auto id : geode::Range( nb_color_components ) )
        {
            const auto band_id = id + 1;
            GDALColorInterp colorInterp =
                gdal_data->GetRasterBand( band_id )->GetColorInterpretation();
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
        return rgb_indices;
    }

    template < typename SpecificRange >
    geode::RasterImage2D read_file( std::string_view filename )
    {
        GDALDatasetUniquePtr gdal_data{ GDALDataset::Open(
            geode::to_string( filename ).c_str(), GDAL_OF_RASTER ) };
        OPENGEODE_EXCEPTION(
            gdal_data, "[ImageInputImpl] Failed to load ", filename );

        const auto width = gdal_data->GetRasterXSize();
        const auto height = gdal_data->GetRasterYSize();
        geode::RasterImage2D raster{ { static_cast< geode::index_t >( width ),
            static_cast< geode::index_t >( height ) } };
        const auto nb_color_components =
            static_cast< geode::index_t >( gdal_data->GetRasterCount() );
        if( nb_color_components <= 2 )
        {
            const auto grey_scale =
                read_color_component( raster, gdal_data, 1 );
            geode::index_t cell{ 0 };
            for( const auto image_j :
                SpecificRange{ raster.nb_cells_in_direction( 1 ) } )
            {
                for( const auto image_i :
                    geode::Range{ raster.nb_cells_in_direction( 0 ) } )
                {
                    const auto pixel = image_i + width * image_j;
                    const auto value = grey_scale[pixel];
                    raster.set_color( cell++, { value, value, value } );
                }
            }
        }
        else if( nb_color_components <= 4 )
        {
            const auto rgb_indices = get_rgb_indices( gdal_data );
            const auto red =
                read_color_component( raster, gdal_data, rgb_indices[0] );
            const auto green =
                read_color_component( raster, gdal_data, rgb_indices[1] );
            const auto blue =
                read_color_component( raster, gdal_data, rgb_indices[2] );
            geode::index_t cell{ 0 };
            for( const auto image_j :
                SpecificRange{ raster.nb_cells_in_direction( 1 ) } )
            {
                for( const auto image_i :
                    geode::Range{ raster.nb_cells_in_direction( 0 ) } )
                {
                    const auto pixel = image_i + width * image_j;
                    raster.set_color(
                        cell++, { red[pixel], green[pixel], blue[pixel] } );
                }
            }
        }
        return raster;
    }
} // namespace

namespace geode
{
    namespace internal
    {
        ImageInputImpl::ImageInputImpl( std::string_view filename )
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
    } // namespace internal
} // namespace geode