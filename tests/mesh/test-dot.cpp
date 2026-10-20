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

#include <filesystem>
#include <fstream>

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>

#include <geode/mesh/builder/polygonal_surface_builder.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/io/polygonal_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

bool save_throws(
    const geode::PolygonalSurface3D& surface, std::string_view filename )
{
    try
    {
        geode::save_polygonal_surface( surface, filename );
    }
    catch( const geode::OpenGeodeException& )
    {
        return true;
    }
    return false;
}

std::string first_line( std::string_view filename )
{
    std::ifstream file{ geode::to_string( filename ) };
    std::string line;
    std::getline( file, line );
    return line;
}

void test_graph_name()
{
    auto surface = geode::PolygonalSurface3D::create();
    auto builder = geode::PolygonalSurfaceBuilder3D::create( *surface );
    builder->create_vertices( 3 );
    builder->create_polygon( { 0, 1, 2 } );
    geode::save_polygonal_surface( *surface, "unnamed.dot" );
    const auto unnamed = first_line( "unnamed.dot" );
    geode::OpenGeodeIOMeshException::test(
        unnamed == absl::StrCat( "graph \"", surface->id().string(), "\" {" ),
        "[TEST] Wrong graph identifier: ", unnamed );
    geode::OpenGeodeIOMeshException::test( !surface->are_edges_enabled(),
        "[TEST] Saving should not enable edges on the surface" );

    builder->set_name( "my \"surface\"" );
    geode::save_polygonal_surface( *surface, "named.dot" );
    const auto named = first_line( "named.dot" );
    geode::OpenGeodeIOMeshException::test(
        named == "graph \"my \\\"surface\\\"\" {",
        "[TEST] Wrong graph identifier: ", named );

    // A trailing backslash must not escape the closing quote
    builder->set_name( "my\\surface\\" );
    geode::save_polygonal_surface( *surface, "backslash.dot" );
    const auto backslash = first_line( "backslash.dot" );
    geode::OpenGeodeIOMeshException::test(
        backslash == "graph \"my\\\\surface\\\\\" {",
        "[TEST] Wrong graph identifier: ", backslash );
}

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();

        auto surface = geode::PolygonalSurface3D::create();
        auto builder = geode::PolygonalSurfaceBuilder3D::create( *surface );
        builder->create_vertices( 3 );
        builder->create_polygon( { 0, 1, 2 } );
        geode::save_polygonal_surface( *surface, "test.dot" );
        test_graph_name();

        // A directory cannot be opened as an output file
        std::filesystem::create_directories( "directory.dot" );
        geode::OpenGeodeIOMeshException::test(
            save_throws( *surface, "directory.dot" ),
            "[TEST] Saving into a directory should throw" );
#ifdef __linux__
        // Writing into /dev/full always fails with ENOSPC
        std::filesystem::remove( "full.dot" );
        std::filesystem::create_symlink( "/dev/full", "full.dot" );
        geode::OpenGeodeIOMeshException::test(
            save_throws( *surface, "full.dot" ),
            "[TEST] Saving into a full device should throw" );
#endif

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
