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
#include <optional>
#include <string>

#include <absl/strings/str_cat.h>
#include <absl/types/span.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/input.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>
#include <geode/basic/variable_attribute.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/core/mesh_factory.hpp>
#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/io/point_set_input.hpp>
#include <geode/mesh/io/point_set_output.hpp>

#include <geode/io/mesh/common.hpp>

namespace
{
    std::string write_pnt( std::string_view name, std::string_view content )
    {
        const auto filename = absl::StrCat( name, ".pnt" );
        std::ofstream file{ filename, std::ios::binary };
        file << content;
        return filename;
    }

    std::unique_ptr< geode::PointSet3D > read_pnt( const std::string& filename )
    {
        auto reader =
            geode::PointSetInputFactory< 3 >::create( "pnt", filename );
        return geode::load_one( reader->read( geode::MeshFactory::default_impl(
                                    geode::PointSet3D::type_name_static() ) ),
            filename );
    }

    std::optional<
        std::shared_ptr< geode::VariableAttribute< geode::index_t > > >
        find_attribute( geode::PointSet3D& point_set, std::string_view name )
    {
        auto& manager = point_set.vertex_attribute_manager();
        const auto ids = manager.attribute_ids_matching_name( name );
        if( !ids )
        {
            return std::nullopt;
        }
        geode::OpenGeodeIOMeshException::test( ids->size() == 1,
            "[TEST: PNT input] Attribute ", name, " should exist once" );
        return manager
            .find_attribute< geode::VariableAttribute, geode::index_t >(
                ids->front() );
    }

    void check_values( geode::PointSet3D& point_set,
        std::string_view name,
        absl::Span< const geode::index_t > expected )
    {
        const auto attribute = find_attribute( point_set, name );
        geode::OpenGeodeIOMeshException::test( attribute.has_value(),
            "[TEST: PNT input] Attribute ", name, " should exist" );
        for( const auto vertex : geode::Indices{ expected } )
        {
            const auto value = attribute.value()->value( vertex );
            geode::OpenGeodeIOMeshException::test( value == expected[vertex],
                "[TEST: PNT input] Wrong ", name, " on vertex ", vertex, ": ",
                value, " instead of ", expected[vertex] );
        }
    }

    void check_read_error(
        const std::string& filename, std::string_view expected_message )
    {
        geode::OpenGeodeIOMeshException::test(
            geode::is_point_set_loadable< 3 >( filename ).value() == 0, "[",
            filename, "] File should not be loadable" );
        try
        {
            read_pnt( filename );
        }
        catch( const geode::OpenGeodeException& exception )
        {
            const std::string message{ exception.what() };
            geode::OpenGeodeIOMeshException::test(
                message.find( expected_message ) != std::string::npos, "[",
                filename, "] Wrong error message: ", message, " (expected \"",
                expected_message, "\")" );
            return;
        }
        geode::OpenGeodeIOMeshException::test(
            false, "[", filename, "] Reading should have failed" );
    }

    void test_contours()
    {
        const auto filepath =
            absl::StrCat( geode::DATA_PATH, "mount_st_helens_contours.pnt" );
        geode::OpenGeodeIOMeshException::test(
            geode::is_point_set_loadable< 3 >( filepath ).value() == 1,
            "[TEST: PNT input] The point set should be loadable" );
        geode::OpenGeodeIOMeshException::test(
            !geode::point_set_additional_files< 3 >( filepath )
                .has_additional_files(),
            "[TEST: PNT input] No additional file should be missing" );
        auto point_set =
            geode::load_one( geode::load_point_set< 3 >( filepath ), filepath );
        geode::OpenGeodeIOMeshException::test( point_set->nb_vertices() == 2603,
            "[TEST: PNT input] ", point_set->nb_vertices(),
            " vertices instead of 2603" );
        geode::OpenGeodeIOMeshException::test(
            point_set->point( 0 )
                == geode::Point3D{ { 562825.315116, 5118000., 1700. } },
            "[TEST: PNT input] Wrong first point" );
        const auto last_vertex = point_set->nb_vertices() - 1;
        geode::OpenGeodeIOMeshException::test(
            find_attribute( *point_set, "polyline" )
                    .value()
                    ->value( last_vertex )
                == 35,
            "[TEST: PNT input] There should be 36 polylines" );
        check_values( *point_set, "colour", { 7 } );
        geode::OpenGeodeIOMeshException::test(
            !find_attribute( *point_set, "mtype" ).has_value(),
            "[TEST: PNT input] Attribute mtype should not exist" );
        geode::save_point_set(
            *point_set, "mount_st_helens_contours.og_pts3d" );
    }

    void test_polylines()
    {
        auto point_set =
            read_pnt( write_pnt( "pnt_polylines", "* Map3D PNT\r\n"
                                                  "\r\n"
                                                  "0 0 0 2\r\n"
                                                  "1 0 0 2 \r\n"
                                                  "  * indented comment\r\n"
                                                  "\t2 0 0\t3\t5\t1\r\n"
                                                  "3 0 0 1\r\n"
                                                  "   \r\n"
                                                  "4 0 0 3 15\r\n" ) );
        geode::OpenGeodeIOMeshException::test( point_set->nb_vertices() == 5,
            "[TEST: PNT polylines] ", point_set->nb_vertices(),
            " vertices instead of 5" );
        geode::OpenGeodeIOMeshException::test(
            point_set->point( 2 ) == geode::Point3D{ { 2, 0, 0 } },
            "[TEST: PNT polylines] Wrong third point" );
        check_values( *point_set, "polyline", { 0, 0, 1, 1, 2 } );
        check_values( *point_set, "flag", { 2, 2, 3, 1, 3 } );
        check_values(
            *point_set, "colour", { geode::NO_ID, geode::NO_ID, 5, 5, 15 } );
        check_values( *point_set, "mtype",
            { geode::NO_ID, geode::NO_ID, 1, 1, geode::NO_ID } );
    }

    void test_invalid_files()
    {
        check_read_error( write_pnt( "pnt_missing_flag", "0 0 0 3\n1 1 1\n" ),
            "Line 2 of file pnt_missing_flag.pnt: expected \"x y z flag\"" );
        check_read_error(
            write_pnt( "pnt_invalid_coordinate", "* header\n0 abc 0 3\n" ),
            "Line 2 of file pnt_invalid_coordinate.pnt: invalid coordinate" );
        check_read_error( write_pnt( "pnt_invalid_flag", "0 0 0 4\n" ),
            "flag should be 1, 2 or 3, found 4" );
        check_read_error( write_pnt( "pnt_invalid_colour", "0 0 0 3 red\n" ),
            "invalid colour \"red\"" );
        geode::OpenGeodeIOMeshException::test(
            geode::is_point_set_loadable< 3 >(
                write_pnt( "pnt_only_comments", "* nothing\n\n" ) )
                    .value()
                == 0,
            "[TEST: PNT input] File without point should not be loadable" );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        geode::Logger::set_level( geode::Logger::LEVEL::trace );
        test_contours();
        test_polylines();
        test_invalid_files();
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
