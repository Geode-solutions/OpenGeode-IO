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

#include <filesystem>
#include <fstream>

#include <geode/tests_config.hpp>

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/polygonal_surface_builder.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/io/polygonal_surface_input.hpp>
#include <geode/mesh/io/polygonal_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

#include <geode/image/core/raster_image.hpp>
#include <geode/mesh/core/texture2d.hpp>
#include <geode/mesh/core/texture_manager.hpp>

namespace
{
    void check_adjacencies( const geode::PolygonalSurface3D& surface )
    {
        auto reference = surface.clone();
        geode::PolygonalSurfaceBuilder3D::create( *reference )
            ->compute_polygon_adjacencies();
        geode::index_t nb_adjacencies{ 0 };
        for( const auto p : geode::Range{ surface.nb_polygons() } )
        {
            for( const auto e : geode::LRange{ surface.nb_polygon_edges( p ) } )
            {
                const auto adjacent = surface.polygon_adjacent( { p, e } );
                geode::OpenGeodeIOMeshException::test(
                    adjacent == reference->polygon_adjacent( { p, e } ),
                    "Wrong adjacency on polygon ", p, " edge ", e );
                if( adjacent )
                {
                    nb_adjacencies++;
                }
            }
        }
        geode::OpenGeodeIOMeshException::test(
            nb_adjacencies > 0, "Loaded surface should have adjacencies" );
    }
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

    void test_duplicated_polygon_textures()
    {
        auto surface = geode::load_polygonal_surface< 3 >(
            absl::StrCat( geode::DATA_PATH, "duplicated_textured.obj" ) );
        geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 1,
            "Number of polygons in duplicated_textured is not correct" );
        const std::array< CornerUV, 3 > expected{ {
            { geode::Point3D{ { 0, 0, 0 } }, geode::Point2D{ { 0.1, 0.2 } } },
            { geode::Point3D{ { 1, 0, 0 } }, geode::Point2D{ { 0.3, 0.4 } } },
            { geode::Point3D{ { 0, 1, 0 } }, geode::Point2D{ { 0.5, 0.6 } } },
        } };
        for( const auto& texture_name :
            { "first_material", "second_material" } )
        {
            geode::OpenGeodeIOMeshException::test(
                check_texture_coordinates( *surface, texture_name, expected )
                    == 3,
                "Wrong number of textured corners in duplicated_textured" );
        }
    }

    void test_lines_and_points()
    {
        auto surface = geode::load_polygonal_surface< 3 >(
            absl::StrCat( geode::DATA_PATH, "lines_points.obj" ) );
        geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 1,
            "Lines and points should not be loaded as polygons" );
        geode::OpenGeodeIOMeshException::test( surface->nb_vertices() == 3,
            "Line and point vertices should not be loaded" );
    }

    void test_too_large_polygon()
    {
        const auto filename = "too_large_polygon.obj";
        {
            constexpr geode::index_t nb_vertices{ 300 };
            std::ofstream file{ filename };
            for( const auto v : geode::Range{ nb_vertices } )
            {
                file << "v " << v << " " << v * v << " 0\n";
            }
            file << "v 5 5 5\nv 6 5 5\nv 5 6 5\nf";
            for( const auto v : geode::Range{ nb_vertices } )
            {
                file << " " << v + 1;
            }
            file << "\nf " << nb_vertices + 1 << " " << nb_vertices + 2 << " "
                 << nb_vertices + 3 << "\n";
        }
        auto surface = geode::load_polygonal_surface< 3 >( filename );
        geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 1,
            "Too large polygon should be skipped" );
        geode::OpenGeodeIOMeshException::test( surface->nb_vertices() == 3,
            "Too large polygon vertices should not be loaded" );
    }

    void test_absolute_texture_path()
    {
        {
            std::ofstream obj{ "absolute_texture.obj" };
            obj << "mtllib absolute_texture.mtl\n"
                << "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                << "vt 0 0\nvt 1 0\nvt 0 1\n"
                << "usemtl absolute\nf 1/1 2/2 3/3\n";
            std::ofstream mtl{ "absolute_texture.mtl" };
            mtl << "newmtl absolute\nKd 1 1 1\nmap_Kd "
                << std::filesystem::absolute(
                       absl::StrCat( geode::DATA_PATH, "grid_image.png" ) )
                       .generic_string()
                << "\n";
        }
        auto surface =
            geode::load_polygonal_surface< 3 >( "./absolute_texture.obj" );
        const auto& texture =
            surface->texture_manager().find_texture( "absolute" );
        geode::OpenGeodeIOMeshException::test( texture.image().nb_cells() > 0,
            "Texture image with absolute path should be loaded" );
    }

    void test_additional_files_bare_filename()
    {
        {
            std::ofstream obj{ "bare_additional.obj" };
            obj << "mtllib bare additional.mtl\n"
                << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
            std::ofstream mtl{ "bare additional.mtl" };
            mtl << "newmtl material\n"
                << "map_Kd -clamp on -s 1 1 1 missing texture.png\n";
        }
        const auto files = geode::polygonal_surface_additional_files< 3 >(
            "bare_additional.obj" );
        geode::OpenGeodeIOMeshException::test( files.optional_files.size() == 2,
            "Wrong number of OBJ additional files" );
        geode::OpenGeodeIOMeshException::test(
            files.optional_files[0].filename == "bare additional.mtl"
                && !files.optional_files[0].is_missing,
            "MTL file next to a bare OBJ filename should be found" );
        geode::OpenGeodeIOMeshException::test(
            files.optional_files[1].filename == "missing texture.png"
                && files.optional_files[1].is_missing,
            "Texture file should be read after map_Kd options" );

        {
            std::ofstream obj{ "bare_mtllib.obj" };
            obj << "mtllib\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        }
        geode::OpenGeodeIOMeshException::test(
            geode::polygonal_surface_additional_files< 3 >( "bare_mtllib.obj" )
                .optional_files.empty(),
            "OBJ with an empty mtllib should have no additional file" );
    }

    void test_output_files( const geode::PolygonalSurface3D& surface )
    {
        std::filesystem::remove( "output_files.mtl" );
        const auto files =
            geode::save_polygonal_surface( surface, "output_files.obj" );
        geode::OpenGeodeIOMeshException::test(
            files.size() == 1 && files[0] == "output_files.obj",
            "Wrong OBJ output file list" );
        geode::OpenGeodeIOMeshException::test(
            !std::filesystem::exists( "output_files.mtl" ),
            "OBJ output should not write an unlisted MTL file" );
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

        check_adjacencies( *surface );
        test_untextured_then_textured_groups();
        test_duplicated_polygon_textures();
        test_lines_and_points();
        test_too_large_polygon();
        test_absolute_texture_path();
        test_additional_files_bare_filename();
        test_output_files( *surface );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
