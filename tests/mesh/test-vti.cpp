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

#include <fstream>

#include <geode/tests_config.hpp>

#include <geode/basic/assert.hpp>
#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/variable_attribute.hpp>

#include <geode/geometry/coordinate_system.hpp>
#include <geode/geometry/vector.hpp>

#include <geode/mesh/builder/regular_grid_solid_builder.hpp>
#include <geode/mesh/builder/regular_grid_surface_builder.hpp>
#include <geode/mesh/core/light_regular_grid.hpp>
#include <geode/mesh/core/regular_grid_solid.hpp>
#include <geode/mesh/core/regular_grid_surface.hpp>
#include <geode/mesh/io/light_regular_grid_input.hpp>
#include <geode/mesh/io/light_regular_grid_output.hpp>
#include <geode/mesh/io/regular_grid_input.hpp>
#include <geode/mesh/io/regular_grid_output.hpp>

#include <geode/io/mesh/common.hpp>

void put_attributes_on_grid( const geode::Grid3D& grid )
{
    geode::AttributeValues< geode::index_t > default_values;
    default_values.default_value = geode::NO_ID;
    default_values.no_value = geode::NO_ID;
    geode::AttributeProperties properties;
    properties.assignable = false;
    properties.interpolable = false;
    properties.transferable = true;
    const auto cell_attribute_id =
        grid.cell_attribute_manager()
            .create_attribute< geode::VariableAttribute, geode::index_t >(
                "id", default_values, properties );
    auto att = grid.cell_attribute_manager()
                   .find_attribute< geode::VariableAttribute, geode::index_t >(
                       cell_attribute_id );
    for( const auto c : geode::Range{ grid.nb_cells() } )
    {
        att->set_value( c, c );
    }
    const auto vertex_attribute_id =
        grid.grid_vertex_attribute_manager()
            .create_attribute< geode::VariableAttribute, geode::index_t >(
                "id_vertex", default_values, properties );
    auto att_vertex =
        grid.grid_vertex_attribute_manager()
            .find_attribute< geode::VariableAttribute, geode::index_t >(
                vertex_attribute_id );
    for( const auto c : geode::Range{ grid.nb_grid_vertices() } )
    {
        att_vertex->set_value( c, c );
    }
}

void test_regular_grid( const geode::RegularGrid3D& grid )
{
    geode::save_regular_grid( grid, "test.vti" );
    const auto reload_grid = geode::load_regular_grid< 3 >( "test.vti" );
    geode::OpenGeodeIOMeshException::test(
        grid.nb_cells() == reload_grid->nb_cells(),
        "[TEST] Wrong number of cells." );
    geode::OpenGeodeIOMeshException::test(
        grid.nb_vertices() == reload_grid->nb_vertices(),
        "[TEST] Wrong number of vertices." );
    for( const auto d : geode::LRange{ 3 } )
    {
        geode::OpenGeodeIOMeshException::test(
            grid.nb_cells_in_direction( d )
                == reload_grid->nb_cells_in_direction( d ),
            "[TEST] Wrong number of cells in direction ", d );
        geode::OpenGeodeIOMeshException::test(
            grid.cell_length_in_direction( d )
                == reload_grid->cell_length_in_direction( d ),
            "[TEST] Wrong cell length in direction ", d );
        geode::OpenGeodeIOMeshException::test(
            grid.grid_coordinate_system().direction( d ).inexact_equal(
                reload_grid->grid_coordinate_system().direction( d ) ),
            "[TEST] Wrong direction in direction ", d );
    }
    geode::OpenGeodeIOMeshException::test(
        grid.grid_coordinate_system().origin().inexact_equal(
            reload_grid->grid_coordinate_system().origin() ),
        "[TEST] Wrong origin." );
    geode::save_regular_grid( *reload_grid, "test2.vti" );
}

void test_light_regular_grid( const geode::LightRegularGrid3D& grid )
{
    geode::save_light_regular_grid( grid, "test3.vti" );
    const auto reload_grid = geode::load_light_regular_grid< 3 >( "test3.vti" );
    geode::OpenGeodeIOMeshException::test(
        grid.nb_cells() == reload_grid.nb_cells(),
        "[TEST] Wrong number of cells." );
    geode::OpenGeodeIOMeshException::test(
        grid.nb_grid_vertices() == reload_grid.nb_grid_vertices(),
        "[TEST] Wrong number of vertices." );
    for( const auto d : geode::LRange{ 3 } )
    {
        geode::OpenGeodeIOMeshException::test(
            grid.nb_cells_in_direction( d )
                == reload_grid.nb_cells_in_direction( d ),
            "[TEST] Wrong number of cells in direction ", d );
        geode::OpenGeodeIOMeshException::test(
            grid.cell_length_in_direction( d )
                == reload_grid.cell_length_in_direction( d ),
            "[TEST] Wrong cell length in direction ", d );
        geode::OpenGeodeIOMeshException::test(
            grid.grid_coordinate_system().direction( d ).inexact_equal(
                reload_grid.grid_coordinate_system().direction( d ) ),
            "[TEST] Wrong direction in direction ", d );
    }
    geode::OpenGeodeIOMeshException::test(
        grid.grid_coordinate_system().origin().inexact_equal(
            reload_grid.grid_coordinate_system().origin() ),
        "[TEST] Wrong origin." );
    geode::save_light_regular_grid( reload_grid, "test4.vti" );
}

void test_loadability()
{
    auto grid2d = geode::RegularGrid2D::create();
    auto builder2d = geode::RegularGridBuilder2D::create( *grid2d );
    builder2d->initialize_grid( geode::Point2D{ { 1, 2 } }, { 10, 20 }, 1 );
    geode::save_regular_grid( *grid2d, "loadability2d.vti" );
    auto grid3d = geode::RegularGrid3D::create();
    auto builder3d = geode::RegularGridBuilder3D::create( *grid3d );
    builder3d->initialize_grid(
        geode::Point3D{ { 1, 2, 3 } }, { 10, 20, 30 }, 1 );
    geode::save_regular_grid( *grid3d, "loadability3d.vti" );
    geode::OpenGeodeIOMeshException::test(
        geode::is_regular_grid_loadable< 2 >( "loadability2d.vti" ).value()
            == 1,
        "2D grid should be loadable in 2D" );
    geode::OpenGeodeIOMeshException::test(
        geode::is_regular_grid_loadable< 3 >( "loadability2d.vti" ).value()
            == 0,
        "2D grid should not be loadable in 3D" );
    geode::OpenGeodeIOMeshException::test(
        geode::is_regular_grid_loadable< 2 >( "loadability3d.vti" ).value()
            == 0,
        "3D grid should not be loadable in 2D" );
    geode::OpenGeodeIOMeshException::test(
        geode::is_light_regular_grid_loadable< 2 >( "loadability3d.vti" )
                .value()
            == 0,
        "3D grid should not be loadable as 2D light grid" );
    geode::OpenGeodeIOMeshException::test(
        geode::is_regular_grid_loadable< 3 >( "loadability3d.vti" ).value()
            == 1,
        "3D grid should be loadable in 3D" );
}

std::string vti_file( std::string_view image_attributes )
{
    return absl::StrCat( "<?xml version=\"1.0\"?>\n<VTKFile type=\"ImageData\" "
                         "version=\"1.0\" byte_order=\"LittleEndian\">\n"
                         "<ImageData ",
        image_attributes, ">\n<Piece></Piece>\n</ImageData>\n</VTKFile>\n" );
}

void write_file( std::string_view filename, std::string_view content )
{
    std::ofstream file{ geode::to_string( filename ), std::ios::binary };
    file << content;
}

void check_grid_load_fails(
    std::string_view filename, std::string_view content )
{
    write_file( filename, content );
    try
    {
        geode::Logger::info( "[TEST] Loading malformed file ", filename );
        geode::load_regular_grid< 3 >( filename );
    }
    catch( const geode::OpenGeodeException& )
    {
        return;
    }
    geode::OpenGeodeIOMeshException::test(
        false, "Loading malformed file ", filename, " should fail" );
}

void test_extent()
{
    write_file(
        "extent.vti", vti_file( "WholeExtent=\"2 5 -1 3 0 2\" "
                                "Origin=\"1 2 3\" Spacing=\"1 2 3\"" ) );
    const auto grid = geode::load_regular_grid< 3 >( "extent.vti" );
    const auto light_grid = geode::load_light_regular_grid< 3 >( "extent.vti" );
    const std::array< geode::index_t, 3 > nb_cells{ 3, 4, 2 };
    const geode::Point3D origin{ { 3, 0, 3 } };
    for( const geode::Grid3D* current :
        { static_cast< const geode::Grid3D* >( grid.get() ),
            static_cast< const geode::Grid3D* >( &light_grid ) } )
    {
        for( const auto d : geode::LRange{ 3 } )
        {
            geode::OpenGeodeIOMeshException::test(
                current->nb_cells_in_direction( d ) == nb_cells[d],
                "[TEST] Wrong number of cells in direction ", d );
        }
        geode::OpenGeodeIOMeshException::test(
            current->grid_coordinate_system().origin().inexact_equal( origin ),
            "[TEST] Wrong origin: ",
            current->grid_coordinate_system().origin().string() );
    }
    check_grid_load_fails(
        "missing_extent.vti", vti_file( "WholeExtent=\"0 1 0 1\"" ) );
    check_grid_load_fails(
        "inverted_extent.vti", vti_file( "WholeExtent=\"0 1 0 1 3 2\"" ) );
    check_grid_load_fails( "missing_origin.vti",
        vti_file( "WholeExtent=\"0 1 0 1 0 1\" Origin=\"0 0\"" ) );
}

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();

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

        test_loadability();
        test_extent();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
