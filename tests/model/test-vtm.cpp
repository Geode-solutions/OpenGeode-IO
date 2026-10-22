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

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include <absl/strings/str_cat.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>

#include <geode/geometry/point.hpp>
#include <geode/geometry/vector.hpp>

#include <geode/mesh/builder/regular_grid_solid_builder.hpp>
#include <geode/mesh/builder/regular_grid_surface_builder.hpp>
#include <geode/mesh/core/mesh_factory.hpp>
#include <geode/mesh/core/regular_grid_solid.hpp>
#include <geode/mesh/core/regular_grid_surface.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/mixin/core/corner.hpp>
#include <geode/model/mixin/core/line.hpp>
#include <geode/model/mixin/core/surface.hpp>
#include <geode/model/representation/builder/brep_builder.hpp>
#include <geode/model/representation/builder/section_builder.hpp>
#include <geode/model/representation/core/brep.hpp>
#include <geode/model/representation/core/section.hpp>
#include <geode/model/representation/io/brep_input.hpp>
#include <geode/model/representation/io/brep_output.hpp>
#include <geode/model/representation/io/section_input.hpp>
#include <geode/model/representation/io/section_output.hpp>

#include <geode/io/mesh/common.hpp>
#include <geode/io/model/common.hpp>

namespace
{
    void check_files(
        const std::vector< std::string >& files, geode::index_t nb_components )
    {
        geode::OpenGeodeIOModelException::test(
            files.size() == nb_components + 1,
            "VTM output should list the .vtm file and one file per "
            "component, got ",
            files.size(), " files for ", nb_components, " components" );
        for( const auto& file : files )
        {
            geode::OpenGeodeIOModelException::test(
                std::filesystem::exists( file ), "Listed file ", file,
                " should exist" );
        }
    }
    /// Checks that the files referenced by the .vtm file exist
    void check_vtm_references( std::string_view vtm_filename )
    {
        std::ifstream file{ geode::to_string( vtm_filename ) };
        const std::string content{ std::istreambuf_iterator< char >{ file },
            std::istreambuf_iterator< char >{} };
        constexpr std::string_view FILE_ATTRIBUTE{ "file=\"" };
        geode::index_t nb_references{ 0 };
        auto start = content.find( FILE_ATTRIBUTE );
        while( start != std::string::npos )
        {
            start += FILE_ATTRIBUTE.size();
            const auto end = content.find( '"', start );
            const auto referenced = content.substr( start, end - start );
            geode::OpenGeodeIOModelException::test(
                std::filesystem::exists( referenced ), "Referenced file ",
                referenced, " should exist" );
            nb_references++;
            start = content.find( FILE_ATTRIBUTE, end );
        }
        geode::OpenGeodeIOModelException::test(
            nb_references != 0, "The .vtm file should reference files" );
    }

    /// Saving must not change the global log level seen by other threads
    void test_log_level( const geode::BRep& brep )
    {
        geode::Logger::set_level( geode::Logger::LEVEL::info );
        std::atomic< bool > done{ false };
        std::thread saver{ [&brep, &done] {
            [[maybe_unused]] const auto files =
                geode::save_brep( brep, "log_level.vtm" );
            done = true;
        } };
        bool level_changed{ false };
        while( !done )
        {
            if( geode::Logger::level() != geode::Logger::LEVEL::info )
            {
                level_changed = true;
            }
        }
        saver.join();
        geode::OpenGeodeIOModelException::test( !level_changed,
            "Saving a VTM file should not change the global log level" );
    }

    void test_regular_grids()
    {
        geode::Section section;
        geode::SectionBuilder section_builder{ section };
        const auto& surface = section.surface(
            section_builder.add_surface( geode::MeshFactory::default_impl(
                geode::RegularGrid2D::type_name_static() ) ) );
        section_builder.surface_mesh_builder< geode::RegularGrid2D >( surface )
            ->initialize_grid( geode::Point2D{ { 0, 0 } }, { 2, 2 }, 1 );
        check_files( geode::save_section( section, "grid_section.vtm" ), 1 );
        check_vtm_references( "grid_section.vtm" );

        geode::BRep brep;
        geode::BRepBuilder brep_builder{ brep };
        const auto& block = brep.block(
            brep_builder.add_block( geode::MeshFactory::default_impl(
                geode::RegularGrid3D::type_name_static() ) ) );
        brep_builder.block_mesh_builder< geode::RegularGrid3D >( block )
            ->initialize_grid( geode::Point3D{ { 0, 0, 0 } }, { 2, 2, 2 }, 1 );
        check_files( geode::save_brep( brep, "grid_brep.vtm" ), 1 );
        check_vtm_references( "grid_brep.vtm" );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        geode::OpenGeodeIOModelLibrary::initialize();

        auto brep =
            geode::load_brep( absl::StrCat( geode::DATA_PATH, "mss.og_brep" ) );
        const auto filename = absl::StrCat( "mss.vtm" );
        check_files( geode::save_brep( brep, filename ),
            brep.nb_corners() + brep.nb_lines() + brep.nb_surfaces()
                + brep.nb_blocks() );
        auto section = geode::load_section(
            absl::StrCat( geode::DATA_PATH, "mss_cut_section.og_sctn" ) );
        const auto filename2 = absl::StrCat( "mss_cut_section.vtm" );
        check_files( geode::save_section( section, filename2 ),
            section.nb_corners() + section.nb_lines() + section.nb_surfaces() );
        check_vtm_references( filename );
        check_vtm_references( filename2 );
        test_regular_grids();
        test_log_level( brep );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
