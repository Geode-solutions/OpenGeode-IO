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

#include <geode/tests_config.h>

#include <geode/basic/assert.h>
#include <geode/basic/attribute_manager.h>
#include <geode/basic/logger.h>

#include <geode/image/core/raster_image.h>
#include <geode/image/core/rgb_color.h>
#include <geode/image/io/raster_image_input.h>
#include <geode/image/io/raster_image_output.h>

#include <geode/mesh/core/regular_grid_surface.h>
#include <geode/mesh/io/regular_grid_input.h>
#include <geode/mesh/io/regular_grid_output.h>

#include <geode/io/image/common.h>
#include <geode/io/mesh/common.h>

void test_jpg_from_gimp_input()
{
    auto raster = geode::load_raster_image< 2 >(
        absl::StrCat( geode::data_path, "grid_image_from_gimp.jpg" ) );
    auto grid = geode::load_regular_grid< 2 >(
        absl::StrCat( geode::data_path, "grid_from_gimp_image.og_rgd2d" ) );
    OPENGEODE_EXCEPTION( raster.nb_cells() == grid->nb_cells(),
        "[TEST] Wrong number of cells." );
    auto comparison_attribute =
        grid->cell_attribute_manager().find_attribute< geode::RGBColor >(
            "RGB_data" );
    for( const auto cell_id : geode::Range{ raster.nb_cells() } )
    {
        OPENGEODE_EXCEPTION(
            raster.color( cell_id ) == comparison_attribute->value( cell_id ),
            "[TEST] Wrong color value for pixel ", cell_id,
            " on image loaded from grid_image_from_gimp.jpg." );
    }

    geode::save_raster_image( raster, "test_grid_output_from_gimp_jpg.vti" );
}

void test_jpg_from_paraview_input()
{
    auto raster = geode::load_raster_image< 2 >(
        absl::StrCat( geode::data_path, "grid_image_from_paraview.jpg" ) );
    auto grid = geode::load_regular_grid< 2 >(
        absl::StrCat( geode::data_path, "grid_from_paraview_image.og_rgd2d" ) );
    OPENGEODE_EXCEPTION( raster.nb_cells() == grid->nb_cells(),
        "[TEST] Wrong number of cells." );
    auto comparison_attribute =
        grid->cell_attribute_manager().find_attribute< geode::RGBColor >(
            "RGB_data" );
    for( const auto cell_id : geode::Range{ raster.nb_cells() } )
    {
        OPENGEODE_EXCEPTION(
            raster.color( cell_id ) == comparison_attribute->value( cell_id ),
            "[TEST] Wrong color value for pixel ", cell_id,
            " on image loaded from grid_image_from_paraview.jpg." );
    }

    geode::save_raster_image(
        raster, "test_grid_output_from_paraview_jpg.vti" );
}

void test_png_input()
{
    auto raster = geode::load_raster_image< 2 >(
        absl::StrCat( geode::data_path, "grid_image.png" ) );
    auto grid = geode::load_regular_grid< 2 >(
        absl::StrCat( geode::data_path, "grid_from_image.og_rgd2d" ) );
    OPENGEODE_EXCEPTION( raster.nb_cells() == grid->nb_cells(),
        "[TEST] Wrong number of cells." );
    auto comparison_attribute =
        grid->cell_attribute_manager().find_attribute< geode::RGBColor >(
            "RGB_data" );
    for( const auto cell_id : geode::Range{ raster.nb_cells() } )
    {
        OPENGEODE_EXCEPTION(
            raster.color( cell_id ) == comparison_attribute->value( cell_id ),
            "[TEST] Wrong color value for pixel ", cell_id,
            " on image loaded from grid_image.png." );
    }

    geode::save_raster_image( raster, "test_grid_output_from_png.vti" );
}

int main()
{
    try
    {
        geode::OpenGeodeMeshLibrary::initialize();
        geode::IOImageLibrary::initialize();

        test_jpg_from_gimp_input();
        test_jpg_from_paraview_input();
        test_png_input();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
