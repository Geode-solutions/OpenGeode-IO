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

#include <geode/basic/assert.hpp>
#include <geode/basic/input.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/geometry/point.hpp>

#include <geode/io/mesh/common.hpp>
#include <geode/io/mesh/csv_input_helpers.hpp>
#include <geode/mesh/core/mesh_factory.hpp>
#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/io/point_set_input.hpp>
#include <geode/mesh/io/point_set_output.hpp>

namespace
{
    std::string write_csv( std::string_view name,
        std::string_view csv_content,
        std::string_view json_content )
    {
        const auto filename = absl::StrCat( name, ".csv" );
        {
            std::ofstream file{ filename, std::ios::binary };
            file << csv_content;
        }
        std::ofstream json{ absl::StrCat( name, ".json" ), std::ios::binary };
        json << json_content;
        return filename;
    }

    std::unique_ptr< geode::PointSet3D > read_csv( const std::string& filename )
    {
        // Use the reader directly: load_point_set hides the error message
        auto reader =
            geode::PointSetInputFactory< 3 >::create( "csv", filename );
        return reader->read( geode::MeshFactory::default_impl(
            geode::PointSet3D::type_name_static() ) );
    }

    void check_read_error(
        const std::string& filename, std::string_view expected_message )
    {
        try
        {
            read_csv( filename );
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

    constexpr auto VALID_SETTINGS = R"({
  "firstRow": 1,
  "headerRow": 0,
  "separator": ",",
  "xColumn": 0,
  "yColumn": 1,
  "zColumn": 2
})";

    void check_nb_vertices( const geode::PointSet3D& point_set,
        geode::index_t expected,
        std::string_view test )
    {
        geode::OpenGeodeIOMeshException::test(
            point_set.nb_vertices() == expected, "[", test,
            "] Wrong number of vertices: ", point_set.nb_vertices(),
            " instead of ", expected );
    }

    void test_blank_and_short_rows()
    {
        const auto point_set = read_csv( write_csv( "csv_short_rows",
            "x,y,z,v\r\n"
            "0,0,0,1\r\n"
            "\r\n"
            "1,1\r\n"
            "   \r\n"
            "2,2,2,3\r\n"
            "3,3,3\r\n",
            VALID_SETTINGS ) );
        check_nb_vertices( *point_set, 3, "short rows" );
        geode::OpenGeodeIOMeshException::test(
            point_set->point( 2 ) == geode::Point3D{ { 3, 3, 3 } },
            "[short rows] Wrong last point" );
    }

    void test_invalid_settings()
    {
        constexpr auto CSV = "x,y,z\n1,2,3\n";
        check_read_error( write_csv( "csv_no_separator", CSV, R"({
  "firstRow": 1, "headerRow": 0, "xColumn": 0, "yColumn": 1, "zColumn": 2
})" ),
            "\"separator\" is missing in settings file csv_no_separator.json" );
        check_read_error( write_csv( "csv_empty_separator", CSV, R"({
  "firstRow": 1, "headerRow": 0, "separator": "",
  "xColumn": 0, "yColumn": 1, "zColumn": 2
})" ),
            "\"separator\" of settings file csv_empty_separator.json" );
        check_read_error( write_csv( "csv_negative_column", CSV, R"({
  "firstRow": 1, "headerRow": 0, "separator": ",",
  "xColumn": -1, "yColumn": 1, "zColumn": 2
})" ),
            "\"xColumn\" of settings file csv_negative_column.json" );
        const auto malformed =
            write_csv( "csv_malformed_json", CSV, R"({ "firstRow": 1, )" );
        check_read_error( malformed, "csv_malformed_json.json" );
        geode::OpenGeodeIOMeshException::test(
            geode::is_point_set_loadable< 3 >( malformed ).value() == 0,
            "[TEST: CSV input] Malformed JSON should not be loadable" );
        geode::OpenGeodeIOMeshException::test(
            !geode::point_set_additional_files< 3 >( malformed )
                .has_additional_files(),
            "[TEST: CSV input] Malformed JSON is not missing" );
    }

    void test_csv_input()
    {
        const auto filepath =
            absl::StrCat( geode::DATA_PATH, "top_granitoid_vertices.csv" );
        const auto additional_files =
            geode::point_set_additional_files< 3 >( filepath );
        const auto is_loadable = geode::is_point_set_loadable< 3 >( filepath );
        geode::OpenGeodeIOMeshException::test( is_loadable.value() == 1,
            "[TEST: CSV input], The point set should be loadable" );
        geode::OpenGeodeIOMeshException::test(
            !additional_files.has_additional_files(),
            "[TEST: CSV input], No additional file should be missing" );
        auto point_set = geode::load_point_set< 3 >( filepath );
        geode::OpenGeodeIOMeshException::test(
            point_set->nb_vertices() == 103455,
            "[TEST: CSV input], "
            "The point set should have 103455 vertices found",
            point_set->nb_vertices() );
        geode::save_point_set( *point_set, "result.og_pts3d" );
    }

    void test_csv_input_with_missing_json()
    {
        const auto filepath =
            absl::StrCat( geode::DATA_PATH, "other_geological_pointset3d.csv" );
        const auto additional_files =
            geode::point_set_additional_files< 3 >( filepath );
        const auto is_loadable = geode::is_point_set_loadable< 3 >( filepath );
        geode::OpenGeodeIOMeshException::test( is_loadable.value() == 0,
            "[TEST: CSV input], The point set should not be loadable" );
        geode::OpenGeodeIOMeshException::test(
            additional_files.has_additional_files(),
            "[TEST: CSV input], The JSON file should be reported missing" );
    }

    void test_csv_input_with_missing_keyword()
    {
        const auto filepath =
            absl::StrCat( geode::DATA_PATH, "missing_keyword.csv" );
        const auto additional_files =
            geode::point_set_additional_files< 3 >( filepath );
        const auto is_loadable = geode::is_point_set_loadable< 3 >( filepath );
        geode::OpenGeodeIOMeshException::test( is_loadable.value() == 0,
            "[TEST: CSV input], The point set should not be loadable because "
            "of a missing keyword" );
        geode::OpenGeodeIOMeshException::test(
            !additional_files.has_additional_files(),
            "[TEST: CSV input], The JSON file is present and should not be "
            "reported missing" );
    }

    void test_csv_input_with_wrong_value()
    {
        const auto filepath =
            absl::StrCat( geode::DATA_PATH, "wrong_value.csv" );
        const auto additional_files =
            geode::point_set_additional_files< 3 >( filepath );
        const auto is_loadable = geode::is_point_set_loadable< 3 >( filepath );
        geode::OpenGeodeIOMeshException::test( is_loadable.value() == 0,
            "[TEST: CSV input], The point set should not be loadable because "
            "of a wrong value" );
        geode::OpenGeodeIOMeshException::test(
            !additional_files.has_additional_files(),
            "[TEST: CSV input], The JSON file is present and should not be "
            "reported missing" );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        geode::Logger::set_level( geode::Logger::LEVEL::trace );
        test_csv_input();
        test_csv_input_with_missing_json();
        test_csv_input_with_missing_keyword();
        test_csv_input_with_wrong_value();
        test_invalid_settings();
        test_blank_and_short_rows();
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}