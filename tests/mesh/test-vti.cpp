/*
 * Copyright (c) 2019 - 2024 Geode-solutions
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
#include <geode/basic/attribute_manager.h>
#include <geode/basic/logger.h>

#include <geode/geometry/coordinate_system.h>
#include <geode/geometry/vector.h>

#include <geode/mesh/builder/regular_grid_solid_builder.h>
#include <geode/mesh/core/light_regular_grid.h>
#include <geode/mesh/core/regular_grid_solid.h>
#include <geode/mesh/io/light_regular_grid_input.h>
#include <geode/mesh/io/light_regular_grid_output.h>
#include <geode/mesh/io/regular_grid_input.h>
#include <geode/mesh/io/regular_grid_output.h>

#include <geode/io/mesh/common.h>

void put_attributes_on_grid( const geode::Grid3D& grid )
{
    auto att = grid.cell_attribute_manager()
                   .find_or_create_attribute< geode::VariableAttribute,
                       geode::index_t >( "id", geode::NO_ID );
    for( const auto c : geode::Range{ grid.nb_cells() } )
    {
        att->set_value( c, c );
    }
    auto att_vertex = grid.grid_vertex_attribute_manager()
                          .find_or_create_attribute< geode::VariableAttribute,
                              geode::index_t >( "id_vertex", geode::NO_ID );
    for( const auto c : geode::Range{ grid.nb_grid_vertices() } )
    {
        att_vertex->set_value( c, c );
    }
}

void test_regular_grid( const geode::RegularGrid3D& grid )
{
    geode::save_regular_grid( grid, "test.vti" );
    const auto reload_grid = geode::load_regular_grid< 3 >( "test.vti" );
    OPENGEODE_EXCEPTION( grid.nb_cells() == reload_grid->nb_cells(),
        "[TEST] Wrong number of cells." );
    OPENGEODE_EXCEPTION( grid.nb_vertices() == reload_grid->nb_vertices(),
        "[TEST] Wrong number of vertices." );
    for( const auto d : geode::LRange{ 3 } )
    {
        OPENGEODE_EXCEPTION( grid.nb_cells_in_direction( d )
                                 == reload_grid->nb_cells_in_direction( d ),
            "[TEST] Wrong number of cells in direction ", d );
        OPENGEODE_EXCEPTION( grid.cell_length_in_direction( d )
                                 == reload_grid->cell_length_in_direction( d ),
            "[TEST] Wrong cell length in direction ", d );
        OPENGEODE_EXCEPTION(
            grid.grid_coordinate_system().direction( d ).inexact_equal(
                reload_grid->grid_coordinate_system().direction( d ) ),
            "[TEST] Wrong direction in direction ", d );
    }
    OPENGEODE_EXCEPTION( grid.grid_coordinate_system().origin().inexact_equal(
                             reload_grid->grid_coordinate_system().origin() ),
        "[TEST] Wrong origin." );
    geode::save_regular_grid( *reload_grid, "test2.vti" );
}

void test_light_regular_grid( const geode::LightRegularGrid3D& grid )
{
    geode::save_light_regular_grid( grid, "test3.vti" );
    const auto reload_grid = geode::load_light_regular_grid< 3 >( "test3.vti" );
    OPENGEODE_EXCEPTION( grid.nb_cells() == reload_grid.nb_cells(),
        "[TEST] Wrong number of cells." );
    OPENGEODE_EXCEPTION(
        grid.nb_grid_vertices() == reload_grid.nb_grid_vertices(),
        "[TEST] Wrong number of vertices." );
    for( const auto d : geode::LRange{ 3 } )
    {
        OPENGEODE_EXCEPTION( grid.nb_cells_in_direction( d )
                                 == reload_grid.nb_cells_in_direction( d ),
            "[TEST] Wrong number of cells in direction ", d );
        OPENGEODE_EXCEPTION( grid.cell_length_in_direction( d )
                                 == reload_grid.cell_length_in_direction( d ),
            "[TEST] Wrong cell length in direction ", d );
        OPENGEODE_EXCEPTION(
            grid.grid_coordinate_system().direction( d ).inexact_equal(
                reload_grid.grid_coordinate_system().direction( d ) ),
            "[TEST] Wrong direction in direction ", d );
    }
    OPENGEODE_EXCEPTION( grid.grid_coordinate_system().origin().inexact_equal(
                             reload_grid.grid_coordinate_system().origin() ),
        "[TEST] Wrong origin." );
    geode::save_light_regular_grid( reload_grid, "test4.vti" );
}

int main()
{
    try
    {
        geode::IOMeshLibrary::initialize();

        auto grid = geode::RegularGrid3D::create();
        auto builder = geode::RegularGridBuilder3D::create( *grid );
        builder->initialize_grid(
            geode::Point3D{ { 1, 2, 3 } }, { 10, 20, 30 }, 1 );
        put_attributes_on_grid( *grid );
        test_regular_grid( *grid );

        geode::LightRegularGrid3D lgrid{ geode::Point3D{ { 1, 2, 3 } },
            { 10, 20, 30 }, { 1, 1, 1 } };
        put_attributes_on_grid( lgrid );
        test_light_regular_grid( lgrid );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
