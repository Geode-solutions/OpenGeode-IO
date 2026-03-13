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
#include <geode/basic/range.hpp>

#include <geode/mesh/core/edged_curve.hpp>
#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/core/polyhedral_solid.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/mixin/core/corner.hpp>
#include <geode/model/mixin/core/line.hpp>
#include <geode/model/mixin/core/surface.hpp>
#include <geode/model/representation/core/brep.hpp>
#include <geode/model/representation/io/brep_input.hpp>
#include <geode/model/representation/io/brep_output.hpp>

#include <geode/io/model/common.hpp>

void test_brep( const geode::BRep& brep,
    geode::index_t nb_corners,
    geode::index_t nb_lines,
    geode::index_t nb_surfaces,
    geode::index_t nb_blocks )
{
    // Number of components
    OPENGEODE_EXCEPTION( brep.nb_corners() == nb_corners,
        "[Test] Number of corners is not correct" );
    OPENGEODE_EXCEPTION(
        brep.nb_lines() == nb_lines, "[Test] Number of lines is not correct" );
    OPENGEODE_EXCEPTION( brep.nb_surfaces() == nb_surfaces,
        "[Test] Number of surfaces is not correct" );
    OPENGEODE_EXCEPTION( brep.nb_blocks() == nb_blocks,
        "[Test] Number of blocks is not correct" );

    // Number of vertices and elements in components
    for( const auto& c : brep.corners() )
    {
        OPENGEODE_EXCEPTION( c.mesh().nb_vertices() == 1,
            "[Test] Number of vertices in corners should be 1" );
    }
    for( const auto& l : brep.lines() )
    {
        OPENGEODE_EXCEPTION( l.mesh().nb_vertices() > 0,
            "[Test] Number of vertices in lines should not be null" );
        OPENGEODE_EXCEPTION( l.mesh().nb_edges() > 0,
            "[Test] Number of edges in lines should not be null" );
    }
    for( const auto& s : brep.surfaces() )
    {
        const auto& mesh = s.mesh();
        OPENGEODE_EXCEPTION( mesh.nb_vertices() > 0,
            "[Test] Number of vertices in surfaces should not be null" );
        OPENGEODE_EXCEPTION( mesh.nb_polygons() > 0,
            "[Test] Number of polygons in surfaces should not be null" );
        geode::index_t count{ 0 };
        for( const auto p : geode::Range{ mesh.nb_polygons() } )
        {
            for( const auto e : geode::LRange{ mesh.nb_polygon_edges( p ) } )
            {
                if( mesh.is_edge_on_border( { p, e } ) )
                    count++;
            }
        }
        OPENGEODE_EXCEPTION( count != 0, "[Test] No polygon adjacency" );
    }

    for( const auto& b : brep.blocks() )
    {
        const auto& mesh = b.mesh();
        OPENGEODE_EXCEPTION( mesh.nb_vertices() > 0,
            "[Test] Number of vertices in blocks should not be null" );
        OPENGEODE_EXCEPTION( mesh.nb_polyhedra() > 0,
            "[Test] Number of polyhedra in blocks should not be null" );
        geode::index_t count{ 0 };
        for( const auto p : geode::Range{ mesh.nb_polyhedra() } )
        {
            for( const auto f :
                geode::LRange{ mesh.nb_polyhedron_facets( p ) } )
            {
                if( mesh.is_polyhedron_facet_on_border( { p, f } ) )
                    count++;
            }
        }
        OPENGEODE_EXCEPTION( count != 0, "[Test] No polyhedron adjacency" );
    }
}

void test_brep_mss( const geode::BRep& brep )
{
    test_brep( brep, 14, 23, 13, 2 );
}

using test_function = void ( * )( const geode::BRep& );

void run_test( std::string_view short_filename, test_function test )
{
    // Load file
    auto brep = geode::load_brep(
        absl::StrCat( geode::DATA_PATH, short_filename, ".msh" ) );
    test( brep );

}

int main()
{
    try
    {
        geode::IOModelLibrary::initialize();

        run_test( "mss", &test_brep_mss );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
