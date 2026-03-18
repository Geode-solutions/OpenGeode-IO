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
namespace
{
    struct BrepDescription
    {
        const geode::BRep& brep;
        geode::index_t nb_corners;
        geode::index_t nb_lines;
        geode::index_t nb_surfaces;
        geode::index_t nb_blocks;
    };

    void check_count_components( const BrepDescription& brep_description )
    {
        OPENGEODE_EXCEPTION(
            brep_description.brep.nb_corners() == brep_description.nb_corners,
            "[Test] Number of corners is not correct" );
        OPENGEODE_EXCEPTION(
            brep_description.brep.nb_lines() == brep_description.nb_lines,
            "[Test] Number of lines is not correct" );
        OPENGEODE_EXCEPTION(
            brep_description.brep.nb_surfaces() == brep_description.nb_surfaces,
            "[Test] Number of surfaces is not correct" );
        OPENGEODE_EXCEPTION(
            brep_description.brep.nb_blocks() == brep_description.nb_blocks,
            "[Test] Number of blocks is not correct" );
    }
    void check_corners( const geode::BRep& brep )
    {
        for( const auto& corner : brep.corners() )
        {
            OPENGEODE_EXCEPTION( corner.mesh().nb_vertices() == 1,
                "[Test] Number of vertices in corners should be 1" );
        }
    }
    void check_lines( const geode::BRep& brep )
    {
        for( const auto& line : brep.lines() )
        {
            const auto& mesh = line.mesh();
            OPENGEODE_EXCEPTION( mesh.nb_vertices() > 0,
                "[Test] Number of vertices in lines should not be null" );
            OPENGEODE_EXCEPTION( mesh.nb_edges() > 0,
                "[Test] Number of edges in lines should not be null" );
        }
    }
    geode::index_t count_surface_edge_on_border(
        const geode::SurfaceMesh3D& mesh )
    {
        geode::index_t count{ 0 };
        for( const auto polygon : geode::Range{ mesh.nb_polygons() } )
        {
            for( const auto edge :
                geode::LRange{ mesh.nb_polygon_edges( polygon ) } )
            {
                if( mesh.is_edge_on_border( { polygon, edge } ) )
                {
                    count++;
                }
            }
        }
        return count;
    }

    void check_surfaces( const geode::BRep& brep )
    {
        for( const auto& surface : brep.surfaces() )
        {
            const auto& mesh = surface.mesh();
            OPENGEODE_EXCEPTION( mesh.nb_vertices() > 0,
                "[Test] Number of vertices in surfaces should not be null" );
            OPENGEODE_EXCEPTION( mesh.nb_polygons() > 0,
                "[Test] Number of polygons in surfaces should not be null" );
            OPENGEODE_EXCEPTION( count_surface_edge_on_border( mesh ) != 0,
                "[Test] No polygon adjacency" );
        }
    }
    geode::index_t count_polyhedron_facet_on_borders(
        const geode::SolidMesh3D& mesh )
    {
        geode::index_t count{ 0 };
        for( const auto polyhedron : geode::Range{ mesh.nb_polyhedra() } )
        {
            for( const auto facet :
                geode::LRange{ mesh.nb_polyhedron_facets( polyhedron ) } )
            {
                if( mesh.is_polyhedron_facet_on_border(
                        { polyhedron, facet } ) )
                {
                    count++;
                }
            }
        }
        return count;
    }

    void check_blocks( const geode::BRep& brep )
    {
        for( const auto& block : brep.blocks() )
        {
            const auto& mesh = block.mesh();
            OPENGEODE_EXCEPTION( mesh.nb_vertices() > 0,
                "[Test] Number of vertices in blocks should not be null" );
            OPENGEODE_EXCEPTION( mesh.nb_polyhedra() > 0,
                "[Test] Number of polyhedra in blocks should not be null" );
            OPENGEODE_EXCEPTION( count_polyhedron_facet_on_borders( mesh ) != 0,
                "[Test] No polyhedron adjacency" );
        }
    }

    void test_brep( const BrepDescription& brep_description )
    {
        check_count_components( brep_description );
        check_corners( brep_description.brep );
        check_lines( brep_description.brep );
        check_surfaces( brep_description.brep );
        check_blocks( brep_description.brep );
    }

    void test_brep_mss( const geode::BRep& brep )
    {
        // NOLINTBEGIN(*-magic-numbers)
        BrepDescription mss_description{ brep, 14, 23, 13, 2 };
        // NOLINTEND(*-magic-numbers)
        test_brep( mss_description );
    }

    using test_function = void ( * )( const geode::BRep& );

    void run_test( std::string_view short_filename, test_function test )
    {
        // Load file
        auto brep = geode::load_brep(
            absl::StrCat( geode::DATA_PATH, short_filename, ".og_brep" ) );
        test( brep );
        const auto filename_gid =
            absl::StrCat( short_filename, "_output.gid.msh" );
        geode::save_brep( brep, filename_gid );
    }
} // namespace

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
