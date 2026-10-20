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

#include <geode/mesh/core/edged_curve.hpp>
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

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
