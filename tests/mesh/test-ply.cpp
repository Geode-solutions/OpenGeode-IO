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

#include <geode/mesh/builder/polygonal_surface_builder.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/io/polygonal_surface_input.hpp>
#include <geode/mesh/io/polygonal_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

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
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        // Load file
        auto surface = geode::load_polygonal_surface< 3 >(
            absl::StrCat( geode::DATA_PATH, "Armadillo.ply" ) );
        geode::OpenGeodeIOMeshException::test( surface->nb_vertices() == 172974,
            "Number of vertices in the loaded Surface is not correct" );
        geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 345944,
            "Number of polygons in the loaded Surface is not correct" );

        check_adjacencies( *surface );

        // Save file
        const auto output_file_og =
            absl::StrCat( "armadillo.", surface->native_extension() );
        geode::save_polygonal_surface( *surface, output_file_og );
        const auto output_file_ply = absl::StrCat( "armadillo.ply" );
        geode::save_polygonal_surface( *surface, output_file_ply );

        // Reload file
        auto reloaded_surface =
            geode::load_polygonal_surface< 3 >( output_file_ply );
        geode::OpenGeodeIOMeshException::test(
            reloaded_surface->nb_vertices() == 172974,
            "Number of vertices in the reloaded Surface is not "
            "correct" );
        geode::OpenGeodeIOMeshException::test(
            reloaded_surface->nb_polygons() == 345944,
            "Number of polygons in the reloaded Surface is not "
            "correct" );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
