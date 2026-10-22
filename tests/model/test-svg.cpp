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

#include <absl/algorithm/container.h>
#include <absl/strings/match.h>
#include <absl/strings/str_cat.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/geometry/point.hpp>

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

void test_load_failure( std::string_view filename, std::string_view path )
{
    try
    {
        const auto section = load_svg_path( filename, path );
    }
    catch( const geode::OpenGeodeException& exception )
    {
        // load_section reports every error as internal: check that the
        // reader detected the problem itself
        geode::OpenGeodeIOModelException::test(
            absl::StrContains( exception.what(), "[SVGInput]" ), "Loading ",
            filename,
            " should fail with an SVGInput error, got: ", exception.what() );
        return;
    }
    throw geode::OpenGeodeIOModelException{ nullptr,
        geode::OpenGeodeException::TYPE::internal, "Loading ", filename,
        " should fail" };
}

std::vector< std::vector< geode::Point2D > > line_points(
    const geode::Section& section )
{
    std::vector< std::vector< geode::Point2D > > lines;
    for( const auto& line : section.lines() )
    {
        auto& points = lines.emplace_back();
        const auto& mesh = line.mesh();
        for( const auto v : geode::Range{ mesh.nb_vertices() } )
        {
            points.push_back( mesh.point( v ) );
        }
    }
    absl::c_sort( lines );
    return lines;
}

void check_lines( std::string_view filename,
    std::string_view path,
    const std::vector< std::vector< geode::Point2D > >& expected )
{
    const auto section = load_svg_path( filename, path );
    auto sorted_expected = expected;
    absl::c_sort( sorted_expected );
    const auto lines = line_points( section );
    geode::OpenGeodeIOModelException::test(
        lines == sorted_expected, "Wrong lines for path \"", path, "\"" );
}

void test_path_grammar()
{
    // Closepath goes back to the subpath start and does not eat the next
    // command
    check_lines( "closepath.svg", "M 0 0 L 1 0 L 1 1 Z M 5 5 L 6 5",
        { { geode::Point2D{ { 0, 0 } }, geode::Point2D{ { 1, 0 } },
              geode::Point2D{ { 1, 1 } }, geode::Point2D{ { 0, 0 } } },
            { geode::Point2D{ { 5, 5 } }, geode::Point2D{ { 6, 5 } } } } );
    // Drawing after a closepath starts from the subpath start
    check_lines( "after_closepath.svg", "M 0 0 L 1 1 Z l 1 0",
        { { geode::Point2D{ { 0, 0 } }, geode::Point2D{ { 1, 1 } },
              geode::Point2D{ { 0, 0 } } },
            { geode::Point2D{ { 0, 0 } }, geode::Point2D{ { 1, 0 } } } } );
    // Repeated closepath
    check_lines( "double_closepath.svg", "M 0 0 L 1 0 L 1 1 Z Z",
        { { geode::Point2D{ { 0, 0 } }, geode::Point2D{ { 1, 0 } },
            geode::Point2D{ { 1, 1 } }, geode::Point2D{ { 0, 0 } } } } );
    // Implicit repetitions keep the command, except after a moveto
    check_lines( "implicit_h.svg", "M 0 0 h 5 10",
        { { geode::Point2D{ { 0, 0 } }, geode::Point2D{ { 5, 0 } },
            geode::Point2D{ { 15, 0 } } } } );
    check_lines( "implicit_v.svg", "M 0 0 V 5 7",
        { { geode::Point2D{ { 0, 0 } }, geode::Point2D{ { 0, 5 } },
            geode::Point2D{ { 0, 7 } } } } );
    check_lines( "implicit_moveto.svg", "m 1 1 2 2",
        { { geode::Point2D{ { 1, 1 } }, geode::Point2D{ { 3, 3 } } } } );

    test_load_failure( "truncated_path.svg", "M 10" );
    test_load_failure( "truncated_lineto.svg", "M 0 0 L 1" );
    test_load_failure( "number_first.svg", "10 10 L 1 1" );
    test_load_failure( "unsupported_command.svg", "M 0 0 C 1 1 2 2 3 3" );
}

void test_compact_numbers()
{
    check_lines( "compact_numbers.svg", "M10-5L.5.5",
        { { geode::Point2D{ { 10, -5 } }, geode::Point2D{ { 0.5, 0.5 } } } } );
    check_lines( "compact_exponents.svg", "M0,0L1e2-1e-1+2E+1-.5",
        { { geode::Point2D{ { 0, 0 } }, geode::Point2D{ { 100, -0.1 } },
            geode::Point2D{ { 20, -0.5 } } } } );
    test_load_failure( "not_a_number.svg", "M 0 0 L 1 0x5" );
}

void test_path_containers()
{
    write_svg( "containers.svg",
        "<path d=\"M 0 0 L 1 0\"/>"
        "<a><path d=\"M 0 1 L 1 1\"/></a>"
        "<svg><g><path d=\"M 0 2 L 1 2\"/></g></svg>"
        "<defs><path id=\"unused\" d=\"M 0 3 L 1 3\"/></defs>" );
    const auto section = geode::load_section( "containers.svg" );
    geode::OpenGeodeIOModelException::test( section.nb_lines() == 3,
        "Paths at the root, in links and in nested svg should be read, "
        "paths in defs should not" );
}

void test_transforms()
{
    write_svg( "transforms.svg",
        "<g transform=\"translate(10,20)\">"
        "<path d=\"M 0 0 L 1 0\" transform=\"scale(2)\"/>"
        "<g transform=\"matrix(1 0 0 1 5 5) scale(3, 1)\">"
        "<path d=\"M 0 1 L 1 1\"/></g></g>"
        "<path d=\"M 1 0 L 2 0\" transform=\"rotate(90)\"/>" );
    const auto lines = line_points( geode::load_section( "transforms.svg" ) );
    std::vector< std::vector< geode::Point2D > > expected{
        { geode::Point2D{ { 10, 20 } }, geode::Point2D{ { 12, 20 } } },
        { geode::Point2D{ { 15, 26 } }, geode::Point2D{ { 18, 26 } } },
        { geode::Point2D{ { 0, 1 } }, geode::Point2D{ { 0, 2 } } }
    };
    absl::c_sort( expected );
    geode::OpenGeodeIOModelException::test(
        lines.size() == expected.size(), "Wrong number of transformed lines" );
    for( const auto l : geode::Indices{ lines } )
    {
        for( const auto p : geode::Indices{ lines[l] } )
        {
            geode::OpenGeodeIOModelException::test(
                lines[l][p].inexact_equal( expected[l][p] ),
                "Wrong transformed point ", lines[l][p].string(), ", expected ",
                expected[l][p].string() );
        }
    }
    write_svg( "bad_transform.svg",
        "<path d=\"M 0 0 L 1 0\" transform=\"translate(1, 2, 3)\"/>" );
    try
    {
        const auto section = geode::load_section( "bad_transform.svg" );
    }
    catch( const geode::OpenGeodeException& /*unused*/ )
    {
        return;
    }
    throw geode::OpenGeodeIOModelException{ nullptr,
        geode::OpenGeodeException::TYPE::internal,
        "Loading an invalid transform should fail" };
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
        test_path_grammar();
        test_compact_numbers();
        test_path_containers();
        test_transforms();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
