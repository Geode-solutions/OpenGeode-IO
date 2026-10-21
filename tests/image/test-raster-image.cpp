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

#include <geode/tests_config.hpp>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>

#include <absl/strings/match.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/logger.hpp>

#include <geode/image/core/raster_image.hpp>
#include <geode/image/core/rgb_color.hpp>
#include <geode/image/io/raster_image_input.hpp>
#include <geode/image/io/raster_image_output.hpp>

#include <geode/mesh/core/regular_grid_surface.hpp>
#include <geode/mesh/io/regular_grid_input.hpp>
#include <geode/mesh/io/regular_grid_output.hpp>

#include <geode/io/image/common.hpp>
#include <geode/io/image/detail/vti_output_impl.hpp>
#include <geode/io/mesh/common.hpp>

void test_jpg_from_gimp_input()
{
    auto raster = geode::load_raster_image< 2 >(
        absl::StrCat( geode::DATA_PATH, "grid_image_from_gimp.jpg" ) );
    auto grid = geode::load_regular_grid< 2 >(
        absl::StrCat( geode::DATA_PATH, "grid_from_gimp_image.og_rgd2d" ) );
    geode::OpenGeodeIOImageException::test(
        raster.nb_cells() == grid->nb_cells(),
        "[TEST] Wrong number of cells." );
    const auto attribute_ids =
        grid->cell_attribute_manager().attribute_ids_matching_name(
            "RGB_data" );
    auto comparison_attribute =
        grid->cell_attribute_manager()
            .find_read_only_attribute< geode::RGBColor >(
                attribute_ids.value().at( 0 ) );
    for( const auto cell_id : geode::Range{ raster.nb_cells() } )
    {
        geode::OpenGeodeIOImageException::test(
            raster.color( cell_id ) == comparison_attribute->value( cell_id ),
            "[TEST] Wrong color value for pixel ", cell_id,
            " on image loaded from grid_image_from_gimp.jpg." );
    }

    geode::save_raster_image( raster, "test_grid_output_from_gimp_jpg.vti" );
}

void test_jpg_from_paraview_input()
{
    auto raster = geode::load_raster_image< 2 >(
        absl::StrCat( geode::DATA_PATH, "grid_image_from_paraview.jpg" ) );
    auto grid = geode::load_regular_grid< 2 >(
        absl::StrCat( geode::DATA_PATH, "grid_from_paraview_image.og_rgd2d" ) );
    geode::OpenGeodeIOImageException::test(
        raster.nb_cells() == grid->nb_cells(),
        "[TEST] Wrong number of cells." );
    const auto attribute_ids =
        grid->cell_attribute_manager().attribute_ids_matching_name(
            "RGB_data" );
    auto comparison_attribute =
        grid->cell_attribute_manager()
            .find_read_only_attribute< geode::RGBColor >(
                attribute_ids.value().at( 0 ) );
    for( const auto cell_id : geode::Range{ raster.nb_cells() } )
    {
        geode::OpenGeodeIOImageException::test(
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
        absl::StrCat( geode::DATA_PATH, "grid_image.png" ) );
    auto grid = geode::load_regular_grid< 2 >(
        absl::StrCat( geode::DATA_PATH, "grid_from_image.og_rgd2d" ) );
    geode::OpenGeodeIOImageException::test(
        raster.nb_cells() == grid->nb_cells(),
        "[TEST] Wrong number of cells." );
    const auto attribute_ids =
        grid->cell_attribute_manager().attribute_ids_matching_name(
            "RGB_data" );
    auto comparison_attribute =
        grid->cell_attribute_manager()
            .find_read_only_attribute< geode::RGBColor >(
                attribute_ids.value().at( 0 ) );
    for( const auto cell_id : geode::Range{ raster.nb_cells() } )
    {
        geode::OpenGeodeIOImageException::test(
            raster.color( cell_id ) == comparison_attribute->value( cell_id ),
            "[TEST] Wrong color value for pixel ", cell_id,
            " on image loaded from grid_image.png." );
    }

    geode::save_raster_image( raster, "test_grid_output_from_png.vti" );
}

void test_tiff_input()
{
    auto raster = geode::load_raster_image< 2 >(
        absl::StrCat( geode::DATA_PATH, "cea.tiff" ) );
    auto ref_raster = geode::load_raster_image< 2 >(
        absl::StrCat( geode::DATA_PATH, "cea.og_img2d" ) );
    geode::OpenGeodeIOImageException::test(
        raster.nb_cells() == ref_raster.nb_cells(),
        "[TEST] Wrong number of cells." );
    for( const auto cell_id : geode::Range{ ref_raster.nb_cells() } )
    {
        geode::OpenGeodeIOImageException::test(
            raster.color( cell_id ) == ref_raster.color( cell_id ),
            "[TEST] Wrong color value for pixel ", cell_id,
            " on image loaded from reference pixel." );
    }

    geode::save_raster_image( raster, "cea.vti" );
}

// Test images are 4x3 pixels, image row 0 being the top row
constexpr geode::index_t TEST_IMAGE_WIDTH = 4;
constexpr geode::index_t TEST_IMAGE_HEIGHT = 3;

std::array< geode::index_t, 2 > test_image_pixel( geode::index_t cell )
{
    return { cell % TEST_IMAGE_WIDTH,
        TEST_IMAGE_HEIGHT - 1 - cell / TEST_IMAGE_WIDTH };
}

geode::RasterImage2D load_test_image( std::string_view filename )
{
    auto raster = geode::load_raster_image< 2 >(
        absl::StrCat( geode::DATA_PATH, filename ) );
    geode::OpenGeodeIOImageException::test(
        raster.nb_cells_in_direction( 0 ) == TEST_IMAGE_WIDTH
            && raster.nb_cells_in_direction( 1 ) == TEST_IMAGE_HEIGHT,
        "[TEST] Wrong size for ", filename );
    return raster;
}

// Colors of the bands of image_rgb files
geode::RGBColor test_image_rgb( geode::index_t cell )
{
    const auto pixel = test_image_pixel( cell );
    return { static_cast< geode::local_index_t >( 20 * pixel[0] ),
        static_cast< geode::local_index_t >( 40 * pixel[1] ), 130 };
}

void check_test_image_colors( std::string_view filename,
    const std::function< geode::RGBColor( geode::index_t ) >& expected )
{
    const auto raster = load_test_image( filename );
    for( const auto cell : geode::Range{ raster.nb_cells() } )
    {
        const auto& color = raster.color( cell );
        const auto expected_color = expected( cell );
        geode::OpenGeodeIOImageException::test( color == expected_color,
            "[TEST] Wrong color for cell ", cell, " of ", filename, ": (",
            color.red(), ", ", color.green(), ", ", color.blue(),
            ") instead of (", expected_color.red(), ", ",
            expected_color.green(), ", ", expected_color.blue(), ")" );
    }
}

void test_bmp_input()
{
    check_test_image_colors( "image_rgb.png", test_image_rgb );
    check_test_image_colors( "image_rgb.bmp", test_image_rgb );
}

void test_untagged_bands_input()
{
    check_test_image_colors( "image_untagged_bands.tif", test_image_rgb );
}

void test_five_bands_input()
{
    check_test_image_colors( "image_five_bands.tif", test_image_rgb );
}

void test_palette_input()
{
    check_test_image_colors( "image_palette.png", []( geode::index_t cell ) {
        const auto pixel = test_image_pixel( cell );
        const auto index = pixel[0] + TEST_IMAGE_WIDTH * pixel[1];
        return geode::RGBColor{ static_cast< geode::local_index_t >(
                                    20 * index ),
            static_cast< geode::local_index_t >( 255 - 20 * index ),
            static_cast< geode::local_index_t >( 5 * index ) };
    } );
}

geode::local_index_t rescaled( double value, double min, double max )
{
    return static_cast< geode::local_index_t >(
        std::round( ( value - min ) * 255. / ( max - min ) ) );
}

void test_non_8_bit_input()
{
    const auto index = []( geode::index_t cell ) {
        const auto pixel = test_image_pixel( cell );
        return pixel[0] + TEST_IMAGE_WIDTH * pixel[1];
    };
    check_test_image_colors( "image_grey16.png", [&index](
                                                     geode::index_t cell ) {
        const auto grey = rescaled( 1000. + 100. * index( cell ), 1000, 2100 );
        return geode::RGBColor{ grey, grey, grey };
    } );
    check_test_image_colors(
        "image_rgb16.tif", [&index]( geode::index_t cell ) {
            const auto value = 100. * index( cell );
            return geode::RGBColor{ rescaled( 1000. + value, 1000, 4100 ),
                rescaled( 2000. + value, 1000, 4100 ),
                rescaled( 3000. + value, 1000, 4100 ) };
        } );
    // Pixel 5 has the nodata value and pixel 6 is NaN
    check_test_image_colors(
        "image_float.tif", [&index]( geode::index_t cell ) {
            const auto pixel = index( cell );
            const auto grey = pixel == 5 || pixel == 6
                                  ? geode::local_index_t{ 0 }
                                  : rescaled( 0.5 * pixel, 0, 5.5 );
            return geode::RGBColor{ grey, grey, grey };
        } );
}

void test_additional_files()
{
    const auto no_files = geode::raster_image_additional_files< 2 >(
        absl::StrCat( geode::DATA_PATH, "image_rgb.png" ) );
    geode::OpenGeodeIOImageException::test(
        no_files.optional_files.empty() && !no_files.has_additional_files(),
        "[TEST] Image without associated file should have no additional "
        "file" );
    const auto files = geode::raster_image_additional_files< 2 >(
        absl::StrCat( geode::DATA_PATH, "image_world_file.png" ) );
    geode::OpenGeodeIOImageException::test(
        files.optional_files.size() == 1 && !files.has_additional_files(),
        "[TEST] Wrong additional files of image with world file" );
    geode::OpenGeodeIOImageException::test(
        absl::EndsWith(
            files.optional_files.front().filename, "image_world_file.pgw" ),
        "[TEST] World file should be an additional file" );
}

void test_empty_image_output()
{
    const geode::RasterImage2D raster;
    geode::save_raster_image( raster, "empty.vti" );
    std::ifstream file{ "empty.vti" };
    const std::string content{ std::istreambuf_iterator< char >{ file },
        std::istreambuf_iterator< char >{} };
    geode::OpenGeodeIOImageException::test(
        absl::StrContains( content, "WholeExtent=\"0 -1 0 -1 0 0\"" ),
        "[TEST] Wrong extent written for empty image" );
}

void test_vti_colors_output()
{
    geode::RasterImage2D raster{ { 3, 2 } };
    for( const auto cell : geode::Range{ raster.nb_cells() } )
    {
        const auto value = static_cast< geode::local_index_t >( 10 * cell );
        raster.set_color(
            cell, { value, static_cast< geode::local_index_t >( value + 1 ),
                      static_cast< geode::local_index_t >( value + 2 ) } );
    }
    geode::save_raster_image( raster, "colors.vti" );
    std::ifstream file{ "colors.vti", std::ios::binary };
    const std::string content{ std::istreambuf_iterator< char >{ file },
        std::istreambuf_iterator< char >{} };
    const auto start = content.find( "<AppendedData encoding=\"raw\">" );
    geode::OpenGeodeIOImageException::test(
        start != std::string::npos, "[TEST] No appended data in VTI file" );
    const auto data = content.find( '_', start ) + 1;
    std::uint64_t nb_bytes{ 0 };
    std::memcpy( &nb_bytes, content.data() + data, sizeof( nb_bytes ) );
    geode::OpenGeodeIOImageException::test(
        nb_bytes == 3 * raster.nb_cells(), "[TEST] Wrong VTI colors size" );
    for( const auto cell : geode::Range{ raster.nb_cells() } )
    {
        const auto* color =
            content.data() + data + sizeof( nb_bytes ) + 3 * cell;
        const auto& expected = raster.color( cell );
        geode::OpenGeodeIOImageException::test(
            static_cast< geode::local_index_t >( color[0] ) == expected.red()
                && static_cast< geode::local_index_t >( color[1] )
                       == expected.green()
                && static_cast< geode::local_index_t >( color[2] )
                       == expected.blue(),
            "[TEST] Wrong VTI color for cell ", cell );
    }
}

class FailingVTIOutput
    : public geode::detail::VTIOutputImpl< geode::RasterImage2D >
{
public:
    FailingVTIOutput(
        const geode::RasterImage2D& raster, std::string_view filename )
        : geode::detail::VTIOutputImpl< geode::RasterImage2D >{ raster,
              filename }
    {
    }

private:
    void write_piece( pugi::xml_node& /*unused*/ ) final
    {
        throw geode::OpenGeodeIOImageException{ nullptr,
            geode::OpenGeodeException::TYPE::data, "Failure while writing" };
    }
};

void test_failed_output_keeps_existing_file()
{
    const auto filename = "existing.vti";
    std::ofstream{ filename } << "previous content";
    const geode::RasterImage2D raster{ { 2, 2 } };
    bool thrown{ false };
    try
    {
        FailingVTIOutput output{ raster, filename };
        output.write_file();
    }
    catch( const geode::OpenGeodeException& )
    {
        thrown = true;
    }
    geode::OpenGeodeIOImageException::test(
        thrown, "[TEST] Failing output should throw" );
    std::ifstream file{ filename };
    const std::string content{ std::istreambuf_iterator< char >{ file },
        std::istreambuf_iterator< char >{} };
    geode::OpenGeodeIOImageException::test( content == "previous content",
        "[TEST] Failing output should keep the existing file" );
    geode::OpenGeodeIOImageException::test(
        !std::filesystem::exists( absl::StrCat( filename, ".tmp" ) ),
        "[TEST] Failing output should not leave a temporary file" );
}

void test_output_through_symbolic_link()
{
#ifdef __linux__
    std::filesystem::remove( "link.vti" );
    std::ofstream{ "link_target.vti" } << "previous content";
    std::filesystem::create_symlink( "link_target.vti", "link.vti" );
    const geode::RasterImage2D raster{ { 2, 2 } };
    geode::save_raster_image( raster, "link.vti" );
    geode::OpenGeodeIOImageException::test(
        std::filesystem::is_symlink( "link.vti" ),
        "[TEST] Output should keep the symbolic link" );
    std::ifstream file{ "link_target.vti" };
    const std::string content{ std::istreambuf_iterator< char >{ file },
        std::istreambuf_iterator< char >{} };
    geode::OpenGeodeIOImageException::test(
        absl::StrContains( content, "VTKFile" ),
        "[TEST] Output should be written in the symbolic link target" );
#endif
}

int main()
{
    try
    {
        geode::OpenGeodeMeshLibrary::initialize();
        geode::OpenGeodeIOImageLibrary::initialize();

        test_jpg_from_gimp_input();
        test_jpg_from_paraview_input();
        test_png_input();
        test_tiff_input();
        test_bmp_input();
        test_untagged_bands_input();
        test_five_bands_input();
        test_palette_input();
        test_non_8_bit_input();
        test_additional_files();
        test_empty_image_output();
        test_failed_output_keeps_existing_file();
        test_output_through_symbolic_link();
        test_vti_colors_output();
        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
