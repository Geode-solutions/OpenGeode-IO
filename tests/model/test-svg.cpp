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

#include <fstream>
#include <string>

#include <absl/strings/str_cat.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/mesh/core/edged_curve.hpp>
#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>

#include <geode/model/mixin/core/corner.hpp>
#include <geode/model/mixin/core/line.hpp>
#include <geode/model/mixin/core/surface.hpp>
#include <geode/model/representation/core/section.hpp>
#include <geode/model/representation/io/section_input.hpp>
#include <geode/model/representation/io/section_output.hpp>

#include <geode/io/model/common.hpp>

geode::index_t nb_closed_lines( const geode::Section& section )
{
    geode::index_t nb{ 0 };
    for( const auto& line : section.lines() )
    {
        if( section.is_closed( line ) )
        {
            nb++;
        }
    }
    return nb;
}

void test_section( const geode::Section& section )
{
    geode::OpenGeodeIOModelException::test(
        section.nb_corners() == 31, "Number of corners is not correct" );
    geode::OpenGeodeIOModelException::test(
        section.nb_lines() == 31, "Number of lines is not correct" );
    geode::OpenGeodeIOModelException::test( nb_closed_lines( section ) == 27,
        "Number of closed lines is not correct" );
    geode::OpenGeodeIOModelException::test(
        section.nb_surfaces() == 0, "Number of surfaces is not correct" );
    for( const auto uv : geode::Range{ section.nb_unique_vertices() } )
    {
        const auto nb_cmv = section.component_mesh_vertices( uv ).size();
        geode::OpenGeodeIOModelException::test( nb_cmv == 1 || nb_cmv == 3,
            "Wrong number of mesh component "
            "vertices for one unique vertex" );
    }
}

void write_svg( std::string_view filename, std::string_view content )
{
    std::ofstream file{ geode::to_string( filename ) };
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\">" << content << "</svg>";
}

geode::Section load_svg_path( std::string_view filename, std::string_view path )
{
    write_svg( filename, absl::StrCat( "<g><path d=\"", path, "\"/></g>" ) );
    return geode::load_section( filename );
}

void test_long_path()
{
    constexpr geode::index_t NB_SEGMENTS{ 20000 };
    std::string path{ "M 0,0" };
    for( const auto s : geode::Range{ 1, NB_SEGMENTS + 1 } )
    {
        absl::StrAppend( &path, "L", s, ",", s % 2 );
    }
    const auto section = load_svg_path( "long_path.svg", path );
    geode::OpenGeodeIOModelException::test(
        section.nb_lines() == 1, "Long path should give one line" );
    for( const auto& line : section.lines() )
    {
        geode::OpenGeodeIOModelException::test(
            line.mesh().nb_vertices() == NB_SEGMENTS + 1,
            "Long path line should have one vertex per point" );
    }
}

int main()
{
    try
    {
        geode::OpenGeodeIOModelLibrary::initialize();

        // Load file
        auto section = geode::load_section(
            absl::StrCat( geode::DATA_PATH, "/logo.svg" ) );
        test_section( section );

        // Save and reload
        const auto filename =
            absl::StrCat( "logo.", section.native_extension() );
        geode::save_section( section, filename );
        auto reloaded_section = geode::load_section( filename );
        test_section( reloaded_section );

        test_long_path();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
