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

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/triangulated_surface_builder.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>
#include <geode/mesh/io/triangulated_surface_input.hpp>
#include <geode/mesh/io/triangulated_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

namespace
{
    void test_large_coordinates()
    {
        auto surface = geode::TriangulatedSurface3D::create();
        auto builder = geode::TriangulatedSurfaceBuilder3D::create( *surface );
        const std::array< geode::Point3D, 3 > points{ {
            geode::Point3D{ { 650000.123, 4100000.456, 100.789 } },
            geode::Point3D{ { 650010.321, 4100000.654, 100.987 } },
            geode::Point3D{ { 650000.111, 4100010.222, 101.333 } },
        } };
        for( const auto& point : points )
        {
            builder->create_point( point );
        }
        builder->create_triangle( { 0, 1, 2 } );
        geode::save_triangulated_surface( *surface, "large_coordinates.stl" );
        const auto reloaded =
            geode::load_triangulated_surface< 3 >( "large_coordinates.stl" );
        geode::OpenGeodeIOMeshException::test( reloaded->nb_vertices() == 3,
            "Wrong number of vertices in reloaded large coordinates STL" );
        for( const auto& point : points )
        {
            bool found{ false };
            for( const auto v : geode::Range{ reloaded->nb_vertices() } )
            {
                if( reloaded->point( v ).inexact_equal( point ) )
                {
                    found = true;
                }
            }
            geode::OpenGeodeIOMeshException::test( found, "Point ",
                point.string(), " lost precision in STL round trip" );
        }
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        // Load file
        auto surface = geode::load_triangulated_surface< 3 >(
            absl::StrCat( geode::DATA_PATH, "thumbwheel.stl" ) );
        geode::OpenGeodeIOMeshException::test( surface->nb_vertices() == 525,
            "Number of vertices in the loaded Surface is not correct" );
        geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 1027,
            "Number of polygons in the loaded Surface is not correct" );

        // Save file
        geode::save_triangulated_surface( *surface,
            absl::StrCat( "thumbwheel.", surface->native_extension() ) );
        const auto output_file_stl = absl::StrCat( "thumbwheel.stl" );
        geode::save_triangulated_surface( *surface, output_file_stl );

        // Reload file
        auto reloaded_surface =
            geode::load_triangulated_surface< 3 >( output_file_stl );
        geode::OpenGeodeIOMeshException::test(
            reloaded_surface->nb_vertices() == 525,
            "Number of vertices in the "
            "reloaded Surface is not correct" );
        geode::OpenGeodeIOMeshException::test(
            reloaded_surface->nb_polygons() == 1027,
            "Number of polygons in the "
            "reloaded Surface is not correct" );

        test_large_coordinates();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
