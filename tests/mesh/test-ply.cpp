/*
 * Copyright (c) 2019 - 2020 Geode-solutions
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
#include <geode/mesh/detail/common.h>
#include <geode/mesh/io/polygonal_surface_input.h>
#include <geode/mesh/io/polygonal_surface_output.h>

int main()
{
    using namespace geode;

    try
    {
        detail::initialize_mesh_io();
        auto surface = PolygonalSurface< 3 >::create();

        // Load file
        load_polygonal_surface(
            *surface, absl::StrCat( data_path, "/Armadillo.ply" ) );
        OPENGEODE_EXCEPTION( surface->nb_vertices() == 172974,
            "[Test] Number of vertices in the loaded Surface is not correct" );
        OPENGEODE_EXCEPTION( surface->nb_polygons() == 345944,
            "[Test] Number of polygons in the loaded Surface is not correct" );

        // Save file
        save_polygonal_surface( *surface,
            absl::StrCat( "armadillo.", surface->native_extension() ) );
        const auto output_file_ply = absl::StrCat( "armadillo.ply" );
        save_polygonal_surface( *surface, output_file_ply );

        // Reload file
        try
        {
            load_polygonal_surface( *surface, output_file_ply );
            OPENGEODE_EXCEPTION( false, "[Test] Exception was not thrown" );
        }
        catch( ... )
        {
            // Exception thrown and catched
        }
        Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode_lippincott();
    }
}
