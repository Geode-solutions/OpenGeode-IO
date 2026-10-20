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
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/core/edged_curve.hpp>
#include <geode/mesh/core/mesh_factory.hpp>
#include <geode/mesh/io/edged_curve_input.hpp>

#include <geode/io/mesh/common.hpp>

namespace
{
    std::unique_ptr< geode::EdgedCurve3D > load_from_string(
        std::string_view name, std::string_view content )
    {
        const auto filename = absl::StrCat( name, ".smesh" );
        {
            std::ofstream file{ filename, std::ios::binary };
            file << content;
        }
        return geode::load_edged_curve< 3 >( filename );
    }

    void check_curve( const geode::EdgedCurve3D& curve,
        geode::index_t nb_vertices,
        geode::index_t nb_edges,
        std::string_view test )
    {
        geode::OpenGeodeIOMeshException::test(
            curve.nb_vertices() == nb_vertices, "[", test,
            "] Wrong number of vertices: ", curve.nb_vertices() );
        geode::OpenGeodeIOMeshException::test( curve.nb_edges() == nb_edges,
            "[", test, "] Wrong number of edges: ", curve.nb_edges() );
    }

    void test_whitespaces()
    {
        const auto curve =
            load_from_string( "smesh_whitespaces", "3  3\t0 0\r\n"
                                                   "0\t0.0  0.0 0.0\r\n"
                                                   "  1 1.0\t\t0.0 0.0  \r\n"
                                                   "2 2.0 0.0 0.0\r\n"
                                                   "2   0\r\n"
                                                   "2 0  1\r\n"
                                                   "2\t1\t2\r\n" );
        check_curve( *curve, 3, 2, "whitespaces" );
    }

    void test_comments()
    {
        const auto curve = load_from_string( "smesh_comments",
            "# Part 1 - node list\n"
            "\n"
            "3 3 0 0 # nb points\n"
            "0 0.0 0.0 0.0\n"
            "   \n"
            "# a comment line\n"
            "1 1.0 0.0 0.0 # inline comment\n"
            "2 2.0 0.0 0.0\n"
            "\n"
            "# Part 2 - facet list\n"
            "2 0\n"
            "2 0 1\n"
            "#2 5 6\n"
            "2 1 2#no space\n"
            "# Part 3 - hole list\n"
            "0\n"
            "# Part 4 - region list\n"
            "0\n" );
        check_curve( *curve, 3, 2, "comments" );
    }

    void check_load_error( std::string_view name,
        std::string_view content,
        std::string_view expected_message )
    {
        // Use the reader directly: load_edged_curve hides the error message
        const auto filename = absl::StrCat( name, ".smesh" );
        {
            std::ofstream file{ filename, std::ios::binary };
            file << content;
        }
        try
        {
            auto reader =
                geode::EdgedCurveInputFactory< 3 >::create( "smesh", filename );
            const auto curve = reader->read( geode::MeshFactory::default_impl(
                geode::EdgedCurve3D::type_name_static() ) );
        }
        catch( const geode::OpenGeodeException& exception )
        {
            const std::string message{ exception.what() };
            const auto found =
                message.find( expected_message ) != std::string::npos;
            geode::OpenGeodeIOMeshException::test( found, "[", name,
                "] Wrong error message: ", message, " (expected \"",
                expected_message, "\")" );
            return;
        }
        geode::OpenGeodeIOMeshException::test(
            false, "[", name, "] Loading should have failed" );
    }

    void test_short_lines()
    {
        check_load_error( "smesh_short_point",
            "3 3 0 0\n"
            "0 0.0 0.0 0.0\n"
            "1 1.0 0.0\n"
            "2 2.0 0.0 0.0\n"
            "2 0\n"
            "2 0 1\n"
            "2 1 2\n",
            "at line 3" );
        check_load_error( "smesh_short_element",
            "3 3 0 0\n"
            "0 0.0 0.0 0.0\n"
            "1 1.0 0.0 0.0\n"
            "2 2.0 0.0 0.0\n"
            "2 0\n"
            "2 0 1\n"
            "2 1\n"
            "2 0 2\n",
            "at line 7" );
        check_load_error( "smesh_bad_number",
            "2 3 0 0\n"
            "0 0.0 0.0 0.0\n"
            "1 1.0 abc 0.0\n"
            "1 0\n"
            "2 0 1\n",
            "at line 3" );
    }

    void test_wrong_element_size()
    {
        check_load_error( "smesh_triangles_as_curve",
            "3 3 0 0\n"
            "0 0.0 0.0 0.0\n"
            "1 1.0 0.0 0.0\n"
            "2 2.0 1.0 0.0\n"
            "1 0\n"
            "3 0 1 2\n",
            "has 2 vertices" );
        const auto curve =
            load_from_string( "smesh_mixed_elements", "3 3 0 0\n"
                                                      "0 0.0 0.0 0.0\n"
                                                      "1 1.0 0.0 0.0\n"
                                                      "2 2.0 1.0 0.0\n"
                                                      "3 0\n"
                                                      "2 0 1\n"
                                                      "3 0 1 2\n"
                                                      "2 1 2\n" );
        check_curve( *curve, 3, 2, "mixed elements" );
    }

    void test_unknown_vertices()
    {
        const auto curve =
            load_from_string( "smesh_unknown_vertex", "3 3 0 0\n"
                                                      "1 0.0 0.0 0.0\n"
                                                      "2 1.0 0.0 0.0\n"
                                                      "3 2.0 1.0 0.0\n"
                                                      "3 0\n"
                                                      "2 1 2\n"
                                                      "2 3 4\n"
                                                      "2 2 3\n" );
        check_curve( *curve, 3, 2, "unknown vertices" );
        check_load_error( "smesh_node_file",
            "0 3 0 0\n"
            "1 0\n"
            "2 1 2\n",
            ".node file" );
    }

    void test_truncated()
    {
        const auto truncated_points =
            load_from_string( "smesh_truncated_points", "5 3 0 0\n"
                                                        "0 0.0 0.0 0.0\n"
                                                        "1 1.0 0.0 0.0\n"
                                                        "2 2.0 1." );
        check_curve( *truncated_points, 2, 0, "truncated points" );
        const auto truncated_elements =
            load_from_string( "smesh_truncated_elements", "3 3 0 0\n"
                                                          "0 0.0 0.0 0.0\n"
                                                          "1 1.0 0.0 0.0\n"
                                                          "2 2.0 1.0 0.0\n"
                                                          "3 0\n"
                                                          "2 0 1\n"
                                                          "2 1" );
        check_curve( *truncated_elements, 3, 1, "truncated elements" );
        // Corrupted header count must not allocate billions of vertices
        const auto huge_count =
            load_from_string( "smesh_huge_count", "4000000000 3 0 0\n"
                                                  "0 0.0 0.0 0.0\n"
                                                  "1 1.0 0.0 0.0\n" );
        check_curve( *huge_count, 2, 0, "huge count" );
    }

    void test_duplicates()
    {
        const auto curve =
            load_from_string( "smesh_duplicates", "4 3 0 0\n"
                                                  "0 0.0 0.0 0.0\n"
                                                  "1 1.0 0.0 0.0\n"
                                                  "1 5.0 5.0 5.0\n"
                                                  "2 2.0 1.0 0.0\n"
                                                  "3 0\n"
                                                  "2 0 1\n"
                                                  "2 2 2\n"
                                                  "2 1 2\n" );
        check_curve( *curve, 3, 2, "duplicates" );
        geode::OpenGeodeIOMeshException::test(
            curve->point( 1 ) == geode::Point3D{ { 1.0, 0.0, 0.0 } },
            "[duplicates] First point with a given id should be kept" );
    }

    void test_is_loadable_samples_elements()
    {
        // is_loadable only samples the first elements: corrupted lines
        // further in the file are reported by the reading itself
        const auto filename = "smesh_is_loadable_sample.smesh";
        {
            std::ofstream file{ filename, std::ios::binary };
            file << "2 3 0 0\n"
                    "0 0.0 0.0 0.0\n"
                    "1 1.0 0.0 0.0\n"
                    "300\n";
            for( const auto e : geode::Range{ 150 } )
            {
                geode_unused( e );
                file << "2 0 1\n";
            }
            for( const auto e : geode::Range{ 150 } )
            {
                geode_unused( e );
                file << "two zero one\n";
            }
        }
        geode::OpenGeodeIOMeshException::test(
            geode::is_edged_curve_loadable< 3 >( filename ).value() == 1,
            "[is_loadable sample] File should be loadable" );
    }
    void check_not_loadable( std::string_view name, std::string_view content )
    {
        const auto filename = absl::StrCat( name, ".smesh" );
        {
            std::ofstream file{ filename, std::ios::binary };
            file << content;
        }
        geode::OpenGeodeIOMeshException::test(
            geode::is_edged_curve_loadable< 3 >( filename ).value() == 0, "[",
            name, "] File should not be loadable" );
    }

    void test_is_loadable_invalid()
    {
        // is_loadable reports invalid files instead of throwing
        check_not_loadable( "smesh_not_loadable_header", "three 3 0 0\n" );
        check_not_loadable( "smesh_not_loadable_element", "2 3 0 0\n"
                                                          "0 0.0 0.0 0.0\n"
                                                          "1 1.0 0.0 0.0\n"
                                                          "1\n"
                                                          "two 0 1\n" );
        // Points stored in a separate .node file are not supported by read
        check_not_loadable( "smesh_not_loadable_no_point", "0 3 0 0\n"
                                                           "1\n"
                                                           "2 0 1\n" );
    }

    void test_load()
    {
        auto curve = geode::load_edged_curve< 3 >(
            absl::StrCat( geode::DATA_PATH, "curve.smesh" ) );
        geode::OpenGeodeIOMeshException::test( curve->nb_vertices() == 1023,
            "Number of vertices in the "
            "loaded EdgedCurve is not correct" );
        geode::OpenGeodeIOMeshException::test( curve->nb_edges() == 1022,
            "Number of edges in the loaded EdgedCurve is not correct" );
    }

    void test_is_loadable()
    {
        geode::OpenGeodeIOMeshException::test(
            geode::is_edged_curve_loadable< 3 >(
                absl::StrCat( geode::DATA_PATH, "curve.smesh" ) )
                    .value()
                == 1,
            "Edge smesh file should be loadable as EdgedCurve" );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        test_load();
        test_is_loadable();
        test_whitespaces();
        test_comments();
        test_short_lines();
        test_wrong_element_size();
        test_unknown_vertices();
        test_truncated();
        test_duplicates();
        test_is_loadable_samples_elements();
        test_is_loadable_invalid();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
