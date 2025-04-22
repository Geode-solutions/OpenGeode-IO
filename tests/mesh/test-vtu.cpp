/*
 * Copyright (c) 2019 - 2025 Geode-solutions
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

#include <geode/mesh/core/hybrid_solid.hpp>
#include <geode/mesh/core/polyhedral_solid.hpp>
#include <geode/mesh/core/tetrahedral_solid.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>
#include <geode/mesh/io/hybrid_solid_input.hpp>
#include <geode/mesh/io/polyhedral_solid_input.hpp>
#include <geode/mesh/io/tetrahedral_solid_input.hpp>
#include <geode/mesh/io/tetrahedral_solid_output.hpp>
#include <geode/mesh/io/triangulated_surface_input.hpp>
#include <geode/mesh/io/triangulated_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

void check( const geode::SolidMesh< 3 >& solid,
    const std::array< geode::index_t, 2 >& test_answers )
{
    OPENGEODE_EXCEPTION( solid.nb_vertices() == test_answers[0],
        "[Test] Number of vertices in the loaded Solid is not correct: "
        "should be ",
        test_answers[0], ", get ", solid.nb_vertices() );
    OPENGEODE_EXCEPTION( solid.nb_polyhedra() == test_answers[1],
        "[Test] Number of polyhedra in the loaded Solid is not correct: "
        "should be ",
        test_answers[1], ", get ", solid.nb_polyhedra() );
}

void check( const geode::SurfaceMesh< 3 >& surface,
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

void run_solid_test( std::string_view filename,
    const std::array< geode::index_t, 2 >& test_answers )
{
    // Load file
    auto solid = geode::load_tetrahedral_solid< 3 >(
        absl::StrCat( geode::DATA_PATH, filename ) );
    check( *solid, test_answers );

    // Save file
    std::string_view filename_without_ext{ filename };
    filename_without_ext.remove_suffix( 4 );
    const auto output_filename =
        absl::StrCat( filename_without_ext, ".", solid->native_extension() );
    geode::save_tetrahedral_solid( *solid, output_filename );

    // Reload file
    auto reload_solid = geode::load_tetrahedral_solid< 3 >( output_filename );
    check( *reload_solid, test_answers );

    // Save file
    const auto output_filename_vtu =
        absl::StrCat( filename_without_ext, "_output.vtu" );
    geode::save_tetrahedral_solid( *solid, output_filename_vtu );

    // Reload file
    auto reload_vtu = geode::load_hybrid_solid< 3 >( output_filename_vtu );
    check( *reload_vtu, test_answers );
}

void run_surface_test( std::string_view filename,
    const std::array< geode::index_t, 2 >& test_answers )
{
    // Load file
    auto surface = geode::load_triangulated_surface< 3 >(
        absl::StrCat( geode::DATA_PATH, filename ) );
    check( *surface, test_answers );

    std::string_view filename_without_ext{ filename };
    filename_without_ext.remove_suffix( 4 );
    geode::save_triangulated_surface(
        *surface, absl::StrCat( filename_without_ext, ".og_tsf3d" ) );
}

int main()
{
    try
    {
        geode::IOMeshLibrary::initialize();
        geode::Logger::set_level( geode::Logger::LEVEL::debug );

        run_solid_test( "cone.vtu", { 580, 2197 } );
        run_solid_test( "cone_append_encoded.vtu", { 580, 2197 } );
        run_surface_test( "mymesh.vtu", { 283308, 564408 } );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
