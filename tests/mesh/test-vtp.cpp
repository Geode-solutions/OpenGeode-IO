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

void check( const geode::PolygonalSurface3D& surface,
    const std::array< geode::index_t, 2 >& test_answers )
{
    OPENGEODE_EXCEPTION( surface.nb_vertices() == test_answers[0],
        "[Test] Number of vertices in the loaded Surface is not correct: "
        "should be ",
        test_answers[0], ", get ", surface.nb_vertices() );
    OPENGEODE_EXCEPTION( surface.nb_polygons() == test_answers[1],
        "[Test] Number of polygons in the loaded Surface is not correct: "
        "should be ",
        test_answers[1], ", get ", surface.nb_polygons() );
}

void run_test( absl::string_view filename,
    const std::array< geode::index_t, 2 >& test_answers )
{
    // Load file
    auto surface = geode::load_polygonal_surface< 3 >(
        absl::StrCat( geode::data_path, filename ) );
    check( *surface, test_answers );

    // Save file
    absl::string_view filename_without_ext{ filename };
    filename_without_ext.remove_suffix( 4 );
    const auto output_filename =
        absl::StrCat( filename_without_ext, ".", surface->native_extension() );
    geode::save_polygonal_surface( *surface, output_filename );

    // Reload file
    auto reload_surface = geode::load_polygonal_surface< 3 >( output_filename );
    check( *reload_surface, test_answers );
}

int main()
{
    try
    {
        geode::detail::initialize_mesh_io();

        run_test( "dfn1_ascii.vtp", { 187, 10 } );
        run_test( "dfn2_mesh_compressed.vtp", { 33413, 58820 } );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
