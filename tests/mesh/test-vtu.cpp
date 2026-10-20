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

#include <geode/mesh/core/hybrid_solid.hpp>
#include <geode/mesh/core/polyhedral_solid.hpp>
#include <geode/mesh/core/tetrahedral_solid.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>
#include <geode/mesh/io/hybrid_solid_input.hpp>
#include <geode/mesh/io/polyhedral_solid_input.hpp>
#include <geode/mesh/io/polyhedral_solid_output.hpp>
#include <geode/mesh/io/tetrahedral_solid_input.hpp>
#include <geode/mesh/io/tetrahedral_solid_output.hpp>
#include <geode/mesh/io/triangulated_surface_input.hpp>
#include <geode/mesh/io/triangulated_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

void check( const geode::SolidMesh< 3 >& solid,
    const std::array< geode::index_t, 2 >& test_answers )
{
    geode::OpenGeodeIOMeshException::test(
        solid.nb_vertices() == test_answers[0],
        "Number of vertices in the loaded Solid is not correct: "
        "should be ",
        test_answers[0], ", get ", solid.nb_vertices() );
    geode::OpenGeodeIOMeshException::test(
        solid.nb_polyhedra() == test_answers[1],
        "Number of polyhedra in the loaded Solid is not correct: "
        "should be ",
        test_answers[1], ", get ", solid.nb_polyhedra() );
}

void check( const geode::SurfaceMesh< 3 >& surface,
    const std::array< geode::index_t, 2 >& test_answers )
{
    geode::OpenGeodeIOMeshException::test(
        surface.nb_vertices() == test_answers[0],
        "Number of vertices in the loaded Surface is not correct: "
        "should be ",
        test_answers[0], ", get ", surface.nb_vertices() );
    geode::OpenGeodeIOMeshException::test(
        surface.nb_polygons() == test_answers[1],
        "Number of polygons in the loaded Surface is not correct: "
        "should be ",
        test_answers[1], ", get ", surface.nb_polygons() );
}

void run_solid_test( std::string_view filename,
    const std::array< geode::index_t, 2 >& test_answers,
    double loadability )
{
    // Load file
    const auto file = absl::StrCat( geode::DATA_PATH, filename );
    auto solid = geode::load_tetrahedral_solid< 3 >( file );
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

    geode::OpenGeodeIOMeshException::test(
        std::fabs( geode::is_tetrahedral_solid_loadable< 3 >( file ).value()
                   - loadability )
            < geode::GLOBAL_EPSILON,
        "File should be loadable" );
}

void run_polyhedral_test( std::string_view filename,
    const std::array< geode::index_t, 2 >& test_answers )
{
    // Load file
    const auto file = absl::StrCat( geode::DATA_PATH, filename );
    auto solid = geode::load_polyhedral_solid< 3 >( file );
    check( *solid, test_answers );

    // Save file
    std::string_view filename_without_ext{ filename };
    filename_without_ext.remove_suffix( 4 );
    const auto output_filename_vtu =
        absl::StrCat( filename_without_ext, "_polyhedral_output.vtu" );
    geode::save_polyhedral_solid( *solid, output_filename_vtu );

    // Reload file
    auto reload_vtu = geode::load_polyhedral_solid< 3 >( output_filename_vtu );
    check( *reload_vtu, test_answers );
}

void run_surface_test( std::string_view filename,
    const std::array< geode::index_t, 2 >& test_answers,
    double loadability )
{
    // Load file
    const auto file = absl::StrCat( geode::DATA_PATH, filename );
    auto surface = geode::load_triangulated_surface< 3 >( file );
    check( *surface, test_answers );

    std::string_view filename_without_ext{ filename };
    filename_without_ext.remove_suffix( 4 );
    geode::save_triangulated_surface(
        *surface, absl::StrCat( filename_without_ext, ".og_tsf3d" ) );

    geode::OpenGeodeIOMeshException::test(
        std::fabs( geode::is_triangulated_surface_loadable< 3 >( file ).value()
                   - loadability )
            < geode::GLOBAL_EPSILON,
        "File should be loadable" );
}

void write_file( std::string_view filename, std::string_view content )
{
    std::ofstream file{ geode::to_string( filename ), std::ios::binary };
    file << content;
}

std::string vtu_file( std::string_view pieces )
{
    return absl::StrCat( "<?xml version=\"1.0\"?>\n<VTKFile "
                         "type=\"UnstructuredGrid\" version=\"1.0\" "
                         "byte_order=\"LittleEndian\">\n<UnstructuredGrid>\n",
        pieces, "</UnstructuredGrid>\n</VTKFile>\n" );
}

// Piece with the 8 vertices of the unit cube, in VTK_VOXEL order
std::string vtu_piece( geode::index_t nb_cells,
    std::string_view connectivity,
    std::string_view offsets,
    std::string_view types,
    std::string_view additional_content = "" )
{
    return absl::StrCat( "<Piece NumberOfPoints=\"8\" NumberOfCells=\"",
        nb_cells,
        "\">\n<Points><DataArray type=\"Float64\" NumberOfComponents=\"3\" "
        "format=\"ascii\">0 0 0 1 0 0 0 1 0 1 1 0 0 0 1 1 0 1 0 1 1 1 1 "
        "1</DataArray></Points>\n<Cells><DataArray type=\"Int64\" "
        "Name=\"connectivity\" format=\"ascii\">",
        connectivity,
        "</DataArray>\n<DataArray type=\"Int64\" Name=\"offsets\" "
        "format=\"ascii\">",
        offsets,
        "</DataArray>\n<DataArray type=\"UInt8\" Name=\"types\" "
        "format=\"ascii\">",
        types, "</DataArray></Cells>\n", additional_content, "</Piece>\n" );
}

void check_surface_load_fails(
    std::string_view filename, std::string_view content )
{
    write_file( filename, content );
    try
    {
        geode::Logger::info( "[TEST] Loading malformed file ", filename );
        geode::load_triangulated_surface< 3 >( filename );
    }
    catch( const geode::OpenGeodeException& )
    {
        return;
    }
    geode::OpenGeodeIOMeshException::test(
        false, "Loading malformed file ", filename, " should fail" );
}

void test_wrong_number_of_types()
{
    check_surface_load_fails( "wrong_number_of_types.vtu",
        vtu_file( vtu_piece( 2, "0 1 2 1 3 2", "3 6", "5" ) ) );
    check_surface_load_fails( "wrong_number_of_offsets.vtu",
        vtu_file( vtu_piece( 2, "0 1 2 1 3 2", "3", "5 5" ) ) );
}

bool has_attribute(
    const geode::AttributeManager& manager, std::string_view name )
{
    const auto ids = manager.attribute_ids_matching_name( name );
    return ids && !ids->empty();
}

void test_cell_data_with_skipped_cells()
{
    // Second cell is a VTK_LINE: it is not loaded in a TriangulatedSurface
    write_file( "cell_data_skipped_cells.vtu",
        vtu_file( vtu_piece( 3, "0 1 2 1 3 1 3 2", "3 5 8", "5 3 5",
            "<PointData><DataArray type=\"Float64\" Name=\"too_short\" "
            "format=\"ascii\">1 2 3</DataArray></PointData>\n"
            "<CellData><DataArray type=\"Int32\" Name=\"id\" "
            "format=\"ascii\">10 20 30</DataArray></CellData>\n" ) ) );
    const auto surface =
        geode::load_triangulated_surface< 3 >( "cell_data_skipped_cells.vtu" );
    check( *surface, { 8, 2 } );
    const auto& manager = surface->polygon_attribute_manager();
    const auto attribute = manager.find_generic_attribute(
        manager.attribute_ids_matching_name( "id" )->at( 0 ) );
    geode::OpenGeodeIOMeshException::test(
        attribute->generic_value( 0 ) == 10.f
            && attribute->generic_value( 1 ) == 30.f,
        "Cell data should be transferred to the loaded triangles" );
    geode::OpenGeodeIOMeshException::test(
        !has_attribute( surface->vertex_attribute_manager(), "too_short" ),
        "Data array with wrong number of values should be skipped" );
}

void test_multiple_pieces()
{
    const auto piece_data = []( std::string_view cell_value ) {
        return absl::StrCat( "<PointData><DataArray type=\"Float64\" "
                             "Name=\"value\" format=\"ascii\">0 1 2 3 4 "
                             "5 6 7</DataArray></PointData>\n<CellData>"
                             "<DataArray type=\"Int32\" Name=\"id\" "
                             "format=\"ascii\">",
            cell_value, "</DataArray></CellData>\n" );
    };
    write_file( "multiple_pieces.vtu",
        vtu_file(
            absl::StrCat( vtu_piece( 1, "0 1 2", "3", "5", piece_data( "10" ) ),
                vtu_piece( 1, "1 3 2", "3", "5", piece_data( "20" ) ) ) ) );
    const auto surface =
        geode::load_triangulated_surface< 3 >( "multiple_pieces.vtu" );
    check( *surface, { 16, 2 } );
    const auto vertices = surface->polygon_vertices( 1 );
    geode::OpenGeodeIOMeshException::test(
        vertices[0] == 9 && vertices[1] == 11 && vertices[2] == 10,
        "Second piece cells should reference second piece vertices" );
    const auto& manager = surface->polygon_attribute_manager();
    const auto ids = manager.attribute_ids_matching_name( "id" );
    geode::OpenGeodeIOMeshException::test( ids && ids->size() == 1,
        "Data arrays with the same name should share one attribute" );
    const auto attribute = manager.find_generic_attribute( ids->at( 0 ) );
    geode::OpenGeodeIOMeshException::test(
        attribute->generic_value( 0 ) == 10.f
            && attribute->generic_value( 1 ) == 20.f,
        "Cell data of each piece should be loaded" );
    const auto& vertex_manager = surface->vertex_attribute_manager();
    const auto vertex_ids =
        vertex_manager.attribute_ids_matching_name( "value" );
    geode::OpenGeodeIOMeshException::test(
        vertex_ids && vertex_ids->size() == 1,
        "Point data arrays with the same name should share one attribute" );
    const auto vertex_attribute =
        vertex_manager.find_generic_attribute( vertex_ids->at( 0 ) );
    geode::OpenGeodeIOMeshException::test(
        vertex_attribute->generic_value( 11 ) == 3.f,
        "Point data of each piece should be loaded" );
}

void test_no_cell()
{
    write_file( "no_cell.vtu", vtu_file( vtu_piece( 0, "", "", "" ) ) );
    geode::OpenGeodeIOMeshException::test(
        geode::is_triangulated_surface_loadable< 3 >( "no_cell.vtu" ).value()
            == 1,
        "VTU file without cell should be loadable as surface" );
    geode::OpenGeodeIOMeshException::test(
        geode::is_tetrahedral_solid_loadable< 3 >( "no_cell.vtu" ).value() == 1,
        "VTU file without cell should be loadable as solid" );
    const auto surface = geode::load_triangulated_surface< 3 >( "no_cell.vtu" );
    check( *surface, { 8, 0 } );
}

void test_cells_with_wrong_number_of_vertices()
{
    // First tetrahedron has only 3 vertices
    write_file( "wrong_cell_size.vtu",
        vtu_file( vtu_piece( 2, "0 1 2 0 1 2 4", "3 7", "10 10",
            "<CellData><DataArray type=\"Int32\" Name=\"id\" "
            "format=\"ascii\">10 20</DataArray></CellData>\n" ) ) );
    const auto solid =
        geode::load_tetrahedral_solid< 3 >( "wrong_cell_size.vtu" );
    check( *solid, { 8, 1 } );
    const auto& manager = solid->polyhedron_attribute_manager();
    const auto attribute = manager.find_generic_attribute(
        manager.attribute_ids_matching_name( "id" )->at( 0 ) );
    geode::OpenGeodeIOMeshException::test(
        attribute->generic_value( 0 ) == 20.f,
        "Cell data should be transferred to the valid tetrahedron" );
}

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        geode::Logger::set_level( geode::Logger::LEVEL::debug );

        test_wrong_number_of_types();
        test_cell_data_with_skipped_cells();
        test_multiple_pieces();
        test_no_cell();
        test_cells_with_wrong_number_of_vertices();

        run_solid_test( "cone.vtu", { 580, 2197 }, 0.624858 );
        run_solid_test( "cone_append_encoded.vtu", { 580, 2197 }, 0.624858 );
        run_solid_test( "cone_append_raw.vtu", { 580, 2197 }, 0.624858 );
        run_polyhedral_test( "cone_append_raw.vtu", { 580, 2197 } );
        run_solid_test( "cone_int32_raw.vtu", { 580, 2197 }, 0.624858 );
        run_surface_test( "cone.vtu", { 580, 1182 }, 0.336177 );
        // One triangle references a vertex out of range and is skipped
        run_surface_test( "mymesh.vtu", { 283308, 564407 }, 1 );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
