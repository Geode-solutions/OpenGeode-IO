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
#include <string>
#include <vector>

#include <geode/tests_config.hpp>

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/triangulated_surface_builder.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>
#include <geode/mesh/io/triangulated_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

namespace
{
    std::unique_ptr< geode::TriangulatedSurface2D > create_surface()
    {
        auto surface = geode::TriangulatedSurface2D::create();
        auto builder = geode::TriangulatedSurfaceBuilder2D::create( *surface );
        builder->create_point( geode::Point2D{ { 0, 0 } } );
        builder->create_point( geode::Point2D{ { 1, 0 } } );
        builder->create_point( geode::Point2D{ { 1, 1 } } );
        builder->create_point( geode::Point2D{ { 0, 1 } } );
        builder->create_triangle( { 0, 1, 2 } );
        builder->create_triangle( { 0, 2, 3 } );
        builder->compute_polygon_adjacencies();
        return surface;
    }

    void test_unwritable_file( const geode::TriangulatedSurface2D& surface )
    {
        // A directory with the .node file name prevents writing it
        std::filesystem::create_directories( "unwritable.node" );
        bool has_thrown{ false };
        try
        {
            geode::save_triangulated_surface( surface, "unwritable.triangle" );
        }
        catch( const geode::OpenGeodeException& )
        {
            has_thrown = true;
        }
        geode::OpenGeodeIOMeshException::test(
            has_thrown, "Writing an unwritable Triangle file should throw" );
    }

    std::vector< std::string > read_lines( const std::string& filename )
    {
        std::ifstream file{ filename };
        std::vector< std::string > lines;
        std::string line;
        while( std::getline( file, line ) )
        {
            lines.push_back( line );
        }
        return lines;
    }

    void test_neighbors( const geode::TriangulatedSurface2D& surface )
    {
        geode::save_triangulated_surface( surface, "neighbors.triangle" );
        const auto lines = read_lines( "neighbors.neigh" );
        // Triangle convention: i-th neighbor is opposite the i-th corner
        geode::OpenGeodeIOMeshException::test(
            lines.size() == 3 && lines[0] == "2 3" && lines[1] == "0 -1 1 -1"
                && lines[2] == "1 -1 -1 0",
            "Wrong neighbors in Triangle .neigh file" );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        const auto surface = create_surface();
        test_unwritable_file( *surface );
        test_neighbors( *surface );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
