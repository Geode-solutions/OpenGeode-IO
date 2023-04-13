/*
 * Copyright (c) 2019 - 2023 Geode-solutions
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

#include <geode/tests_config.h>

#include <geode/basic/assert.h>
#include <geode/basic/logger.h>

#include <geode/mesh/core/polygonal_surface.h>
#include <geode/mesh/io/polygonal_surface_input.h>
#include <geode/mesh/io/polygonal_surface_output.h>

#include <geode/io/mesh/common.h>

#include <geode/image/core/raster_image.h>
#include <geode/mesh/core/texture2d.h>
int main()
{
    try
    {
        geode::IOMeshLibrary::initialize();
        // Load file
        auto surface = geode::load_polygonal_surface< 3 >(
            absl::StrCat( geode::data_path, "TopHat.obj" ) );
        OPENGEODE_EXCEPTION( surface->nb_vertices() == 363,
            "[Test] Number of vertices in the loaded Surface is not correct" );
        OPENGEODE_EXCEPTION( surface->nb_polygons() == 380,
            "[Test] Number of polygons in the loaded Surface is not correct" );

        // Save file
        geode::save_polygonal_surface(
            *surface, absl::StrCat( "TopHat.", surface->native_extension() ) );
        const auto output_file_obj = absl::StrCat( "TopHat.obj" );
        geode::save_polygonal_surface( *surface, output_file_obj );

        // Reload file
        auto reloaded_surface =
            geode::load_polygonal_surface< 3 >( output_file_obj );
        OPENGEODE_EXCEPTION( surface->nb_vertices() == 363,
            "[Test] Number of vertices in the "
            "reloaded Surface is not correct" );
        OPENGEODE_EXCEPTION( surface->nb_polygons() == 380,
            "[Test] Number of polygons in the "
            "reloaded Surface is not correct" );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
