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
#include <geode/mesh/io/polygonal_surface_input.h>
#include <geode/mesh/io/polygonal_surface_output.h>

#include <geode/io/mesh/detail/common.h>

void run_test( absl::string_view filename )
{
    auto surface = geode::PolygonalSurface< 3 >::create();
    // Load file
    load_polygonal_surface(
        *surface, absl::StrCat( geode::data_path, filename ) );
    DEBUG( surface->nb_vertices() );
    DEBUG( surface->nb_polygons() );
    // OPENGEODE_EXCEPTION( surface->nb_vertices() == 172974,
    //     "[Test] Number of vertices in the loaded Surface is not correct" );
    // OPENGEODE_EXCEPTION( surface->nb_polygons() == 345944,
    //     "[Test] Number of polygons in the loaded Surface is not correct" );

    // Save file
    auto filename_without_ext{ filename };
    filename_without_ext.remove_suffix( 4 );
    save_polygonal_surface( *surface, absl::StrCat( filename_without_ext, ".",
                                          surface->native_extension() ) );
    // save_polygonal_surface( *surface, filename );
    // auto reload_surface = geode::PolygonalSurface< 3 >::create();
    // load_polygonal_surface( *reload_surface, filename );
}

int main()
{
    try
    {
        geode::detail::initialize_mesh_io();

        run_test( "dfn1.vtp" );
        DEBUG( "====" );
        // run_test( "dfn1_compressed.vtp" );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
