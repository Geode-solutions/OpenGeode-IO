/*
 * Copyright (c) 2019 Geode-solutions
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

#include <geode/georepresentation/core/boundary_representation.h>
#include <geode/georepresentation/core/corner.h>
#include <geode/georepresentation/core/line.h>
#include <geode/georepresentation/core/surface.h>
#include <geode/georepresentation/core/block.h>
#include <geode/georepresentation/io/boundary_representation_input.h>
#include <geode/georepresentation/io/boundary_representation_output.h>

#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/edged_curve.h>
#include <geode/mesh/core/polygonal_surface.h>
#include <geode/mesh/core/polyhedral_solid.h>

#include <geode/georepresentation/detail/common.h>
#include <geode/georepresentation/detail/msh_input.h>

void test_brep( const geode::BRep& brep )
{
    // Number of components
    OPENGEODE_EXCEPTION(
        brep.nb_corners() == 8, "Number of corners is not correct" );
    OPENGEODE_EXCEPTION(
        brep.nb_lines() == 12, "Number of lines is not correct" );
    OPENGEODE_EXCEPTION(
        brep.nb_surfaces() == 6, "Number of surfaces is not correct" );
    // OPENGEODE_EXCEPTION(
    //     brep.nb_blocks() == 1, "Number of blocks is not correct" );

    // Number of vertices and elements in components
    for( const auto& c : brep.corners() )
    {
        OPENGEODE_EXCEPTION(
            c.mesh().nb_vertices() == 1, "Number of vertices in corners should be 1" );
    }
    for( const auto& l : brep.lines() )
    {
        OPENGEODE_EXCEPTION(
            l.mesh().nb_vertices() == 5, "Number of vertices in lines should be 5" );
        OPENGEODE_EXCEPTION(
            l.mesh().nb_edges() == 4, "Number of edges in lines should be 4" );
    }
    for( const auto& s : brep.surfaces() )
    {
        OPENGEODE_EXCEPTION( s.mesh().nb_vertices() == 29,
            "Number of vertices in surfaces should be 29" );
        OPENGEODE_EXCEPTION( s.mesh().nb_polygons() == 40,
            "Number of polygons in surfaces should be 40" );
    }
}

int main()
{
    using namespace geode;

    try
    {
        initialize_georepresentation_io();
        BRep brep;

        // Load file
        load_brep( brep, test_path + "georepresentation/data/cube_v22.msh" );
        test_brep( brep );

        // Save and reload
        save_brep( brep, test_path + "georepresentation/output/cube_v22."
                             + brep.native_extension() );
        // save_brep( brep, test_path +
        // "georepresentation/output/cube_v22.msh"); BRep reloaded_brep;
        // load_brep( reloaded_brep,test_path +
        // "georepresentation/output/cube_v22.msh"); test_brep( reloaded_brep );

        Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode_lippincott();
    }
}
