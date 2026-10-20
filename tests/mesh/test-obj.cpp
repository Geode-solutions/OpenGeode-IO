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

#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/io/polygonal_surface_input.hpp>
#include <geode/mesh/io/polygonal_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

#include <geode/image/core/raster_image.hpp>
#include <geode/mesh/core/texture2d.hpp>
#include <geode/mesh/core/texture_manager.hpp>

namespace
{
    struct CornerUV
    {
        geode::Point3D point;
        geode::Point2D uv;
    };

    // Checks every polygon corner located on one of the given points has the
    // expected texture coordinates. Returns the number of checked corners.
    geode::index_t check_texture_coordinates(
        const geode::PolygonalSurface3D& surface,
        std::string_view texture_name,
        absl::Span< const CornerUV > expected )
    {
        const auto manager = surface.texture_manager();
        geode::OpenGeodeIOMeshException::test(
            manager.texture_exists( texture_name ), "Texture ", texture_name,
            " should exist" );
        const auto& texture = manager.find_texture( texture_name );
        geode::index_t nb_checked{ 0 };
        for( const auto p : geode::Range{ surface.nb_polygons() } )
        {
            for( const auto v :
                geode::LRange{ surface.nb_polygon_vertices( p ) } )
            {
                const auto& point =
                    surface.point( surface.polygon_vertex( { p, v } ) );
                for( const auto& corner : expected )
                {
                    if( !point.inexact_equal( corner.point ) )
                    {
                        continue;
                    }
                    geode::OpenGeodeIOMeshException::test(
                        texture.texture_coordinates( { p, v } )
                            .inexact_equal( corner.uv ),
                        "Wrong texture coordinates on polygon ", p, " vertex ",
                        v, " of texture ", texture_name );
                    nb_checked++;
                }
            }
        }
        return nb_checked;
    }

    void test_untextured_then_textured_groups()
    {
        auto surface = geode::load_polygonal_surface< 3 >(
            absl::StrCat( geode::DATA_PATH, "two_groups_textured.obj" ) );
        geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 2,
            "Number of polygons in two_groups_textured is not correct" );
        const std::array< CornerUV, 3 > expected{ {
            { geode::Point3D{ { 2, 0, 0 } }, geode::Point2D{ { 0.1, 0.2 } } },
            { geode::Point3D{ { 3, 0, 0 } }, geode::Point2D{ { 0.3, 0.4 } } },
            { geode::Point3D{ { 2, 1, 0 } }, geode::Point2D{ { 0.5, 0.6 } } },
        } };
        geode::OpenGeodeIOMeshException::test(
            check_texture_coordinates( *surface, "textured_material", expected )
                == 3,
            "Wrong number of textured corners in two_groups_textured" );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        // Load file
        auto surface = geode::load_polygonal_surface< 3 >(
            absl::StrCat( geode::DATA_PATH, "TopHat.obj" ) );
        geode::OpenGeodeIOMeshException::test( surface->nb_vertices() == 363,
            "Number of vertices in the loaded Surface is not correct" );
        geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 380,
            "Number of polygons in the loaded Surface is not correct" );

        // Save file
        geode::save_polygonal_surface(
            *surface, absl::StrCat( "TopHat.", surface->native_extension() ) );
        const auto output_file_obj = absl::StrCat( "TopHat.obj" );
        geode::save_polygonal_surface( *surface, output_file_obj );

        // Reload file
        auto reloaded_surface =
            geode::load_polygonal_surface< 3 >( output_file_obj );
        geode::OpenGeodeIOMeshException::test(
            reloaded_surface->nb_vertices() == 363,
            "Number of vertices in the "
            "reloaded Surface is not correct" );
        geode::OpenGeodeIOMeshException::test(
            reloaded_surface->nb_polygons() == 380,
            "Number of polygons in the "
            "reloaded Surface is not correct" );

        test_untextured_then_textured_groups();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
