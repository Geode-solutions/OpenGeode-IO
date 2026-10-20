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

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>

#include <geode/mesh/core/edged_curve.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>
#include <geode/mesh/io/edged_curve_input.hpp>
#include <geode/mesh/io/triangulated_surface_input.hpp>

#include <geode/io/mesh/common.hpp>

namespace
{
    void test_load()
    {
        auto surface = geode::load_triangulated_surface< 3 >(
            absl::StrCat( geode::DATA_PATH, "triangulated.smesh" ) );
        geode::OpenGeodeIOMeshException::test( surface->nb_vertices() == 111,
            "Number of vertices in the loaded Surface is not correct" );
        geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 186,
            "Number of polygons in the loaded Surface is not correct" );
    }

    void test_is_loadable()
    {
        const auto triangle_file =
            absl::StrCat( geode::DATA_PATH, "triangulated.smesh" );
        const auto curve_file = absl::StrCat( geode::DATA_PATH, "curve.smesh" );
        geode::OpenGeodeIOMeshException::test(
            geode::is_triangulated_surface_loadable< 3 >( triangle_file )
                    .value()
                == 1,
            "Triangle smesh file should be loadable as TriangulatedSurface" );
        geode::OpenGeodeIOMeshException::test(
            geode::is_triangulated_surface_loadable< 3 >( curve_file ).value()
                == 0,
            "Edge smesh file should not be loadable as TriangulatedSurface" );
        geode::OpenGeodeIOMeshException::test(
            geode::is_edged_curve_loadable< 3 >( triangle_file ).value() == 0,
            "Triangle smesh file should not be loadable as EdgedCurve" );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        test_load();
        test_is_loadable();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
