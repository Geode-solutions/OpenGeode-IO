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
#include <geode/basic/range.h>

#include <geode/mesh/core/edged_curve.h>
#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/polygonal_surface.h>
#include <geode/mesh/core/polyhedral_solid.h>

#include <geode/model/detail/common.h>
#include <geode/model/mixin/core/block.h>
#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/mixin/core/surface.h>
#include <geode/model/representation/core/brep.h>
#include <geode/model/representation/io/brep_input.h>
#include <geode/model/representation/io/brep_output.h>

void test_brep( const geode::BRep& brep )
{
    // Number of components
    OPENGEODE_EXCEPTION(
        brep.nb_corners() == 8, "[Test] Number of corners is not correct" );
    OPENGEODE_EXCEPTION(
        brep.nb_lines() == 12, "[Test] Number of lines is not correct" );
    OPENGEODE_EXCEPTION(
        brep.nb_surfaces() == 6, "[Test] Number of surfaces is not correct" );
    OPENGEODE_EXCEPTION(
        brep.nb_blocks() == 1, "[Test] Number of blocks is not correct" );

    // Number of vertices and elements in components
    for( const auto& c : brep.corners() )
    {
        OPENGEODE_EXCEPTION( c.mesh().nb_vertices() == 1,
            "[Test] Number of vertices in corners should be 1" );
    }
    for( const auto& l : brep.lines() )
    {
        OPENGEODE_EXCEPTION( l.mesh().nb_vertices() == 5,
            "[Test] Number of vertices in lines should be 5" );
        OPENGEODE_EXCEPTION( l.mesh().nb_edges() == 4,
            "[Test] Number of edges in lines should be 4" );
    }
    for( const auto& s : brep.surfaces() )
    {
        OPENGEODE_EXCEPTION( s.mesh().nb_vertices() == 29,
            "[Test] Number of vertices in surfaces should be 29" );
        OPENGEODE_EXCEPTION( s.mesh().nb_polygons() == 40,
            "[Test] Number of polygons in surfaces should be 40" );
    }

    for( const auto& b : brep.blocks() )
    {
        OPENGEODE_EXCEPTION( b.mesh().nb_vertices() == 131,
            "[Test] Number of vertices in blocks should be 131" );
        OPENGEODE_EXCEPTION( b.mesh().nb_polyhedra() == 364,
            "[Test] Number of polyhedra in blocks should be 364" );
    }

    // Number of component boundaries and incidences
    for( const auto& c : brep.corners() )
    {
        OPENGEODE_EXCEPTION( brep.nb_boundaries( c.id() ) == 0,
            "[Test] Number of corner boundary should be 0" );
        OPENGEODE_EXCEPTION( brep.nb_incidences( c.id() ) == 3,
            "[Test] Number of corner incidences should be 3" );
    }
    for( const auto& l : brep.lines() )
    {
        OPENGEODE_EXCEPTION( brep.nb_boundaries( l.id() ) == 2,
            "[Test] Number of line boundary should be 2" );
        OPENGEODE_EXCEPTION( brep.nb_incidences( l.id() ) == 2,
            "[Test] Number of line incidences should be 2" );
    }
    for( const auto& s : brep.surfaces() )
    {
        OPENGEODE_EXCEPTION( brep.nb_boundaries( s.id() ) == 4,
            "[Test] Number of surface boundary should be 4" );
        OPENGEODE_EXCEPTION( brep.nb_incidences( s.id() ) == 1,
            "[Test] Number of surface incidences should be 1" );
    }
    for( const auto& b : brep.blocks() )
    {
        OPENGEODE_EXCEPTION( brep.nb_boundaries( b.id() ) == 6,
            "[Test] Number of block boundary should be 6" );
        OPENGEODE_EXCEPTION( brep.nb_incidences( b.id() ) == 0,
            "[Test] Number of block incidences should be 0" );
    }
}

int main()
{
    using namespace geode;

    try
    {
        initialize_model_io();
        BRep brep;

        // Load file
        load_brep( brep, absl::StrCat( data_path, "/cube_v22.msh" ) );
        test_brep( brep );

        // Save and reload
        const auto filename =
            absl::StrCat( "model/output/cube_v22.", brep.native_extension() );
        save_brep( brep, filename );
        BRep reloaded_brep;
        load_brep( reloaded_brep, filename );
        test_brep( reloaded_brep );

        Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode_lippincott();
    }
}
