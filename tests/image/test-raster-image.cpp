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

#include <filesystem>
#include <fstream>
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
        test_empty_image_output();
        test_failed_output_keeps_existing_file();
        test_output_through_symbolic_link();
        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
