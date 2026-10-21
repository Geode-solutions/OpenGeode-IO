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

#include <filesystem>

#include <absl/strings/match.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/variable_attribute.hpp>

#include <geode/geometry/point.hpp>

#include <geode/image/core/raster_image.hpp>

#include <geode/mesh/builder/polygonal_surface_builder.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/core/texture2d.hpp>
#include <geode/mesh/core/texture_manager.hpp>
#include <geode/mesh/io/polygonal_surface_input.hpp>
#include <geode/mesh/io/polygonal_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

void check( const geode::PolygonalSurface3D& surface,
    const std::array< geode::index_t, 2 >& test_answers,
    absl::Span< const geode::uuid > vertex_attributes,
    absl::Span< const geode::uuid > polygon_attributes )
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
    for( const auto& id : vertex_attributes )
    {
        geode::OpenGeodeIOMeshException::test(
            surface.vertex_attribute_manager().attribute_exists( id ),
            "Attribute ", id.string(),
            " was not be loaded as attribute on vertices" );
    }
    for( const auto& id : polygon_attributes )
    {
        geode::OpenGeodeIOMeshException::test(
            surface.polygon_attribute_manager().attribute_exists( id ),
            "Attribute ", id.string(),
            " was not be loaded as attribute on polygons" );
    }
}

void run_test( std::string_view filename,
    const std::array< geode::index_t, 2 >& test_answers,
    absl::Span< const geode::uuid > vertex_attributes,
    absl::Span< const geode::uuid > polygon_attributes )
{
    // Load file
    auto surface = geode::load_polygonal_surface< 3 >(
        absl::StrCat( geode::DATA_PATH, filename ) );
    check( *surface, test_answers, vertex_attributes, polygon_attributes );

    // Save file
    std::string_view filename_without_ext{ filename };
    filename_without_ext.remove_suffix( 4 );
    const auto output_filename_default =
        absl::StrCat( filename_without_ext, ".", surface->native_extension() );
    geode::save_polygonal_surface( *surface, output_filename_default );

    // Reload file
    auto reload_surface =
        geode::load_polygonal_surface< 3 >( output_filename_default );
    check(
        *reload_surface, test_answers, vertex_attributes, polygon_attributes );

    // Save file
    const auto output_filename_vtp =
        absl::StrCat( filename_without_ext, "_output.vtp" );
    geode::save_polygonal_surface( *surface, output_filename_vtp );

    // Reload file
    auto reload_surface_vtp =
        geode::load_polygonal_surface< 3 >( output_filename_vtp );
    check( *reload_surface_vtp, test_answers, vertex_attributes,
        polygon_attributes );
}

void check_same_attributes( const geode::AttributeManager& reference,
    const geode::AttributeManager& manager,
    std::string_view filename )
{
    for( const auto& reference_id : reference.attribute_ids() )
    {
        const auto reference_attribute =
            reference.find_generic_attribute( reference_id );
        const auto name = reference_attribute->name().value();
        const auto ids = manager.attribute_ids_matching_name( name );
        geode::OpenGeodeIOMeshException::test( ids && ids->size() == 1,
            "Attribute ", name, " was not loaded from ", filename );
        const auto attribute = manager.find_generic_attribute( ids->at( 0 ) );
        geode::OpenGeodeIOMeshException::test(
            attribute->nb_items() == reference_attribute->nb_items(),
            "Wrong number of items for attribute ", name, " in ", filename );
        for( const auto e : geode::Range{ reference.nb_elements() } )
        {
            for( const auto i : geode::LRange{ attribute->nb_items() } )
            {
                const auto expected =
                    reference_attribute->generic_item_value( e, i );
                const auto value = attribute->generic_item_value( e, i );
                geode::OpenGeodeIOMeshException::test(
                    std::fabs( value - expected )
                        <= 1e-6 * std::max( 1.f, std::fabs( expected ) ),
                    "Wrong value for attribute ", name, " on element ", e,
                    " in ", filename, ": should be ", expected, ", get ",
                    value );
            }
        }
    }
}

void check_same_surface( const geode::PolygonalSurface3D& reference,
    const geode::PolygonalSurface3D& surface,
    std::string_view filename )
{
    check(
        surface, { reference.nb_vertices(), reference.nb_polygons() }, {}, {} );
    for( const auto v : geode::Range{ reference.nb_vertices() } )
    {
        geode::OpenGeodeIOMeshException::test(
            surface.point( v ).inexact_equal( reference.point( v ) ),
            "Wrong point ", v, " in ", filename );
    }
    for( const auto p : geode::Range{ reference.nb_polygons() } )
    {
        geode::OpenGeodeIOMeshException::test(
            surface.polygon_vertices( p ) == reference.polygon_vertices( p ),
            "Wrong polygon ", p, " in ", filename );
    }
    check_same_attributes( reference.vertex_attribute_manager(),
        surface.vertex_attribute_manager(), filename );
    check_same_attributes( reference.polygon_attribute_manager(),
        surface.polygon_attribute_manager(), filename );
}

void run_encoding_test(
    const geode::PolygonalSurface3D& reference, std::string_view filename )
{
    const auto surface = geode::load_polygonal_surface< 3 >(
        absl::StrCat( geode::DATA_PATH, filename ) );
    check_same_surface( reference, *surface, filename );

    const auto output_filename = absl::StrCat( "output_", filename );
    geode::save_polygonal_surface( *surface, output_filename );
    const auto reload_surface =
        geode::load_polygonal_surface< 3 >( output_filename );
    check_same_surface( reference, *reload_surface, output_filename );
}

void run_encoding_tests()
{
    const auto reference = geode::load_polygonal_surface< 3 >(
        absl::StrCat( geode::DATA_PATH, "dfn1_ascii.vtp" ) );
    for( const auto encoding : { "raw", "raw_compressed", "base64",
             "base64_compressed", "binary", "binary_compressed" } )
    {
        run_encoding_test(
            *reference, absl::StrCat( "dfn1_mixed_types_", encoding, ".vtp" ) );
    }
}

template < typename T >
std::shared_ptr< geode::VariableAttribute< T > > create_attribute(
    geode::AttributeManager& manager, std::string_view name, T no_value )
{
    geode::AttributeValues< T > values;
    values.default_value = no_value;
    values.no_value = no_value;
    geode::AttributeProperties properties;
    properties.transferable = true;
    const auto id = manager.create_attribute< geode::VariableAttribute, T >(
        name, values, properties );
    return manager.find_attribute< geode::VariableAttribute, T >( id );
}

void run_attribute_types_test()
{
    auto surface = geode::load_polygonal_surface< 3 >(
        absl::StrCat( geode::DATA_PATH, "dfn1_ascii.vtp" ) );
    auto precise = create_attribute< double >(
        surface->vertex_attribute_manager(), "precise", std::nan( "" ) );
    for( const auto v : geode::Range{ surface->nb_vertices() } )
    {
        precise->set_value( v, 1. + v * 1e-12 );
    }
    auto negative =
        create_attribute< int >( surface->polygon_attribute_manager(),
            "negative", std::numeric_limits< int >::max() );
    for( const auto p : geode::Range{ surface->nb_polygons() } )
    {
        negative->set_value( p, -static_cast< int >( p ) );
    }
    auto optional_id = create_attribute< geode::index_t >(
        surface->polygon_attribute_manager(), "optional_id", geode::NO_ID );
    for( const auto p : geode::Range{ surface->nb_polygons() } )
    {
        if( p % 2 == 0 )
        {
            optional_id->set_value( p, p );
        }
    }
    geode::save_polygonal_surface( *surface, "attribute_types.vtp" );

    const auto reload =
        geode::load_polygonal_surface< 3 >( "attribute_types.vtp" );
    const auto& vertex_manager = reload->vertex_attribute_manager();
    const auto reload_precise =
        vertex_manager.find_read_only_attribute< double >(
            vertex_manager.attribute_ids_matching_name( "precise" )->at( 0 ) );
    for( const auto v : geode::Range{ surface->nb_vertices() } )
    {
        geode::OpenGeodeIOMeshException::test(
            reload_precise->value( v ) == precise->value( v ),
            "Double attribute value was not saved exactly" );
    }
    const auto& polygon_manager = reload->polygon_attribute_manager();
    const auto reload_negative = polygon_manager.find_generic_attribute(
        polygon_manager.attribute_ids_matching_name( "negative" )->at( 0 ) );
    for( const auto p : geode::Range{ surface->nb_polygons() } )
    {
        geode::OpenGeodeIOMeshException::test(
            reload_negative->generic_value( p ) == -static_cast< float >( p ),
            "Negative int attribute value was not saved correctly" );
    }
    const auto reload_optional_id = polygon_manager.find_generic_attribute(
        polygon_manager.attribute_ids_matching_name( "optional_id" )->at( 0 ) );
    for( const auto p : geode::Range{ surface->nb_polygons() } )
    {
        if( p % 2 == 0 )
        {
            geode::OpenGeodeIOMeshException::test(
                reload_optional_id->has_value( p )
                    && reload_optional_id->generic_value( p )
                           == static_cast< float >( p ),
                "Set index attribute value was not saved correctly" );
        }
        else
        {
            geode::OpenGeodeIOMeshException::test(
                !reload_optional_id->has_value( p ),
                "Index attribute without value was not saved as no value" );
        }
    }
}

void test_array_attributes()
{
    auto surface = geode::load_polygonal_surface< 3 >(
        absl::StrCat( geode::DATA_PATH, "dfn1_ascii.vtp" ) );
    auto& manager = surface->polygon_attribute_manager();
    std::array< double, 6 > no_tensor;
    no_tensor.fill( std::nan( "" ) );
    auto tensor = create_attribute< std::array< double, 6 > >(
        manager, "tensor", no_tensor );
    std::array< geode::index_t, 8 > no_ids;
    no_ids.fill( geode::NO_ID );
    auto ids = create_attribute< std::array< geode::index_t, 8 > >(
        manager, "ids", no_ids );
    for( const auto p : geode::Range{ surface->nb_polygons() } )
    {
        std::array< double, 6 > tensor_value;
        for( const auto i : geode::LIndices{ tensor_value } )
        {
            tensor_value[i] = 1. + ( 6 * p + i ) * 1e-12;
        }
        tensor->set_value( p, tensor_value );
        std::array< geode::index_t, 8 > ids_value;
        for( const auto i : geode::LIndices{ ids_value } )
        {
            ids_value[i] = ( 1U << 24 ) + 8 * p + i + 1;
        }
        ids->set_value( p, ids_value );
    }
    geode::save_polygonal_surface( *surface, "array_attributes.vtp" );

    const auto reload =
        geode::load_polygonal_surface< 3 >( "array_attributes.vtp" );
    const auto& reload_manager = reload->polygon_attribute_manager();
    const auto reload_tensor =
        reload_manager.find_read_only_attribute< std::vector< double > >(
            reload_manager.attribute_ids_matching_name( "tensor" )->at( 0 ) );
    const auto reload_ids =
        reload_manager
            .find_read_only_attribute< std::vector< geode::index_t > >(
                reload_manager.attribute_ids_matching_name( "ids" )->at( 0 ) );
    for( const auto p : geode::Range{ surface->nb_polygons() } )
    {
        const auto& tensor_value = tensor->value( p );
        geode::OpenGeodeIOMeshException::test(
            reload_tensor->value( p )
                == std::vector< double >(
                    tensor_value.begin(), tensor_value.end() ),
            "[TEST] std::array< double, 6 > attribute was not saved exactly" );
        const auto& ids_value = ids->value( p );
        geode::OpenGeodeIOMeshException::test(
            reload_ids->value( p )
                == std::vector< geode::index_t >(
                    ids_value.begin(), ids_value.end() ),
            "[TEST] std::array< index_t, 8 > attribute was not saved "
            "exactly" );
    }
}

double temperature_value( double time, geode::index_t vertex )
{
    return 10. * time + vertex;
}

void test_time_series()
{
    auto surface = geode::PolygonalSurface3D::create();
    auto builder = geode::PolygonalSurfaceBuilder3D::create( *surface );
    builder->create_point( geode::Point3D{ { 0, 0, 0 } } );
    builder->create_point( geode::Point3D{ { 1, 0, 0 } } );
    builder->create_point( geode::Point3D{ { 0, 1, 0 } } );
    builder->create_point( geode::Point3D{ { 1, 1, 1 } } );
    builder->create_polygon( { 0, 1, 2 } );
    builder->create_polygon( { 1, 3, 2 } );
    const std::array< double, 3 > creation_times{ 2., 0.5, 1. };
    auto& manager = surface->vertex_attribute_manager();
    for( const auto time : creation_times )
    {
        const auto step =
            manager.find_attribute< geode::VariableAttribute, double >(
                manager.create_time_step_attribute< geode::VariableAttribute,
                    double >( "temperature", time, { 0, 0 }, {} ) );
        for( const auto vertex : geode::Range{ surface->nb_vertices() } )
        {
            step->set_value( vertex, temperature_value( time, vertex ) );
        }
    }
    const std::array< double, 2 > same_name_values{ 1., 2. };
    auto& polygon_manager = surface->polygon_attribute_manager();
    for( const auto value : same_name_values )
    {
        const auto id =
            polygon_manager
                .create_attribute< geode::VariableAttribute, double >(
                    "temperature", { value, 0 }, {} );
    }
    geode::save_polygonal_surface( *surface, "time_series.vtp" );
    const auto reload_surface =
        geode::load_polygonal_surface< 3 >( "time_series.vtp" );
    const auto& reload_manager = reload_surface->vertex_attribute_manager();
    const std::array< double, 3 > sorted_times{ 0.5, 1., 2. };
    for( const auto step : geode::Indices{ sorted_times } )
    {
        const auto name = absl::StrCat( "temperature@", step );
        const auto ids = reload_manager.attribute_ids_matching_name( name );
        geode::OpenGeodeIOMeshException::test(
            ids && ids->size() == 1, "Attribute ", name, " should be written" );
        const auto attribute =
            reload_manager.find_read_only_attribute< double >( ids->front() );
        for( const auto vertex : geode::Range{ reload_surface->nb_vertices() } )
        {
            geode::OpenGeodeIOMeshException::test(
                attribute->value( vertex )
                    == temperature_value( sorted_times[step], vertex ),
                "Wrong value of ", name, " at vertex ", vertex );
        }
    }
    const auto& reload_polygon_manager =
        reload_surface->polygon_attribute_manager();
    for( const auto index : geode::Indices{ same_name_values } )
    {
        const auto name = absl::StrCat( "temperature_", index );
        const auto ids =
            reload_polygon_manager.attribute_ids_matching_name( name );
        const auto attribute =
            reload_polygon_manager.find_read_only_attribute< double >(
                ids->front() );
        for( const auto polygon :
            geode::Range{ reload_surface->nb_polygons() } )
        {
            geode::OpenGeodeIOMeshException::test(
                attribute->value( polygon ) == same_name_values[index],
                "Wrong value of ", name, " at polygon ", polygon );
        }
    }
}

void test_duplicated_names()
{
    auto surface = geode::PolygonalSurface3D::create();
    auto builder = geode::PolygonalSurfaceBuilder3D::create( *surface );
    builder->create_point( geode::Point3D{ { 0, 0, 0 } } );
    builder->create_point( geode::Point3D{ { 1, 0, 0 } } );
    builder->create_point( geode::Point3D{ { 0, 1, 0 } } );
    builder->create_polygon( { 0, 1, 2 } );
    auto& manager = surface->polygon_attribute_manager();
    for( const auto value : { 1., 2. } )
    {
        [[maybe_unused]] const auto id =
            manager.create_attribute< geode::VariableAttribute, double >(
                "value", { value, 0 }, {} );
    }
    [[maybe_unused]] const auto literal_id =
        manager.create_attribute< geode::VariableAttribute, double >(
            "value_0", { 3., 0 }, {} );
    geode::save_polygonal_surface( *surface, "duplicated_names.vtp" );
    const auto reload =
        geode::load_polygonal_surface< 3 >( "duplicated_names.vtp" );
    const auto& reload_manager = reload->polygon_attribute_manager();
    const auto literal = reload_manager.find_read_only_attribute< double >(
        reload_manager.attribute_ids_matching_name( "value_0" )->front() );
    geode::OpenGeodeIOMeshException::test( literal->value( 0 ) == 3.,
        "[TEST] Attribute value_0 should keep its name" );
    std::vector< double > renamed_values;
    for( const auto& id : reload_manager.attribute_ids() )
    {
        const auto attribute = reload_manager.find_generic_attribute( id );
        const auto name = attribute->name().value();
        if( name != "value_0" && absl::StartsWith( name, "value_" ) )
        {
            renamed_values.push_back( attribute->generic_value( 0 ) );
        }
    }
    absl::c_sort( renamed_values );
    geode::OpenGeodeIOMeshException::test(
        renamed_values == std::vector< double >{ 1., 2. },
        "[TEST] Attributes with the same name should get unique names" );
}

void test_texture_files()
{
    auto surface = geode::PolygonalSurface3D::create();
    auto builder = geode::PolygonalSurfaceBuilder3D::create( *surface );
    builder->create_point( geode::Point3D{ { 0, 0, 0 } } );
    builder->create_point( geode::Point3D{ { 1, 0, 0 } } );
    builder->create_point( geode::Point3D{ { 0, 1, 0 } } );
    builder->create_polygon( { 0, 1, 2 } );
    auto& texture =
        surface->texture_manager().find_or_create_texture( "dir/texture" );
    texture.set_image( geode::RasterImage2D{ { 2, 2 } } );
    for( const auto v : geode::LRange{ 3 } )
    {
        texture.set_texture_coordinates(
            { 0, v }, geode::Point2D{ { 0.5 * v, 0.25 * v } } );
    }
    const auto files =
        geode::save_polygonal_surface( *surface, "textured.vtp" );
    geode::OpenGeodeIOMeshException::test( files.size() == 2,
        "[TEST] Texture image file should be returned, got ", files.size(),
        " files" );
    geode::OpenGeodeIOMeshException::test(
        files[1] == "textured_dir_texture.vti",
        "[TEST] Wrong texture image file: ", files[1] );
    geode::OpenGeodeIOMeshException::test( std::filesystem::exists( files[1] ),
        "[TEST] Texture image file should exist" );
}

void test_texture_split_vertices()
{
    auto surface = geode::PolygonalSurface3D::create();
    auto builder = geode::PolygonalSurfaceBuilder3D::create( *surface );
    builder->create_point( geode::Point3D{ { 0, 0, 0 } } );
    builder->create_point( geode::Point3D{ { 1, 0, 0 } } );
    builder->create_point( geode::Point3D{ { 1, 1, 0 } } );
    builder->create_point( geode::Point3D{ { 0, 1, 0 } } );
    // Vertex 1 is used twice by the first polygon
    builder->create_polygon( { 0, 1, 2, 1 } );
    builder->create_polygon( { 0, 2, 3 } );
    auto& texture = surface->texture_manager().find_or_create_texture( "uv" );
    const std::array< std::vector< geode::Point2D >, 2 > uvs{ {
        { geode::Point2D{ { 0, 0 } }, geode::Point2D{ { 1, 0 } },
            geode::Point2D{ { 1, 1 } }, geode::Point2D{ { 0.5, 0.5 } } },
        { geode::Point2D{ { 0, 0 } }, geode::Point2D{ { 0.2, 1 } },
            geode::Point2D{ { 0, 1 } } },
    } };
    for( const auto p : geode::LIndices{ uvs } )
    {
        for( const auto v : geode::LIndices{ uvs[p] } )
        {
            texture.set_texture_coordinates( { p, v }, uvs[p][v] );
        }
    }
    geode::save_polygonal_surface( *surface, "texture_split.vtp" );

    const auto reload =
        geode::load_polygonal_surface< 3 >( "texture_split.vtp" );
    // Vertex 0 is shared with the same UV, vertex 1 has 2 UVs, vertex 2 has
    // 2 UVs: 6 vertices
    geode::OpenGeodeIOMeshException::test( reload->nb_vertices() == 6,
        "[TEST] Wrong number of split vertices: ", reload->nb_vertices() );
    const auto& manager = reload->vertex_attribute_manager();
    const auto reload_uv =
        manager.find_read_only_attribute< std::array< double, 2 > >(
            manager.attribute_ids_matching_name( "uv" )->at( 0 ) );
    for( const auto p : geode::LIndices{ uvs } )
    {
        for( const auto v : geode::LIndices{ uvs[p] } )
        {
            const geode::PolygonVertex pv{ p, v };
            const auto vertex = reload->polygon_vertex( pv );
            geode::OpenGeodeIOMeshException::test(
                reload->point( vertex )
                    == surface->point( surface->polygon_vertex( pv ) ),
                "[TEST] Wrong point for polygon ", p, " corner ", v );
            const auto& uv = reload_uv->value( vertex );
            geode::OpenGeodeIOMeshException::test(
                uv[0] == uvs[p][v].value( 0 ) && uv[1] == uvs[p][v].value( 1 ),
                "[TEST] Wrong texture coordinates for polygon ", p, " corner ",
                v );
        }
    }
}

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

void test_write_errors()
{
    const auto surface = geode::load_polygonal_surface< 3 >(
        absl::StrCat( geode::DATA_PATH, "dfn1_ascii.vtp" ) );
    // A directory cannot be opened as an output file
    std::filesystem::create_directories( "directory.vtp" );
    geode::OpenGeodeIOMeshException::test(
        save_throws( *surface, "directory.vtp" ),
        "[TEST] Saving into a directory should throw" );
#ifdef __linux__
    // Writing into /dev/full always fails with ENOSPC
    std::filesystem::remove( "full.vtp" );
    std::filesystem::create_symlink( "/dev/full", "full.vtp" );
    geode::OpenGeodeIOMeshException::test( save_throws( *surface, "full.vtp" ),
        "[TEST] Saving into a full device should throw" );
#endif
}

void write_file( std::string_view filename, std::string_view content )
{
    std::ofstream file{ geode::to_string( filename ), std::ios::binary };
    file << content;
}

std::string vtp_file( std::string_view header_attributes,
    std::string_view piece_attributes,
    std::string_view piece_content )
{
    return absl::StrCat( "<?xml version=\"1.0\"?>\n<VTKFile type=\"PolyData\" "
                         "version=\"1.0\" byte_order=\"LittleEndian\" ",
        header_attributes, ">\n<PolyData>\n<Piece ", piece_attributes, ">\n",
        piece_content, "</Piece>\n</PolyData>\n</VTKFile>\n" );
}

std::string_view ascii_points()
{
    return "<Points><DataArray type=\"Float64\" NumberOfComponents=\"3\" "
           "format=\"ascii\">0 0 0 1 0 0 0 1 0 1 1 0</DataArray></Points>\n";
}

void check_load_fails( std::string_view filename, std::string_view content )
{
    write_file( filename, content );
    try
    {
        geode::Logger::info( "[TEST] Loading malformed file ", filename );
        geode::load_polygonal_surface< 3 >( filename );
    }
    catch( const geode::OpenGeodeException& )
    {
        return;
    }
    geode::OpenGeodeIOMeshException::test(
        false, "Loading malformed file ", filename, " should fail" );
}

void test_zlib_header_overflow()
{
    // Compressed header with 2 blocks of 2^31 bytes: total size overflows
    // a 32-bit integer
    const auto content = vtp_file(
        "header_type=\"UInt32\" compressor=\"vtkZLibDataCompressor\"",
        "NumberOfPoints=\"3\" NumberOfPolys=\"1\"",
        "<Points><DataArray type=\"Float32\" NumberOfComponents=\"3\" "
        "format=\"binary\">AgAAAAAAAIAAAACAGgAAABoAAAA="
        "eJztwQENAAAAwqD3T20PBxQAAADwbhAAAAF4nO3BAQ0AAADCoPdPbQ8HFAAAAPBuEAAA"
        "AQ==</DataArray></Points>\n"
        "<Polys><DataArray type=\"Int64\" Name=\"connectivity\" "
        "format=\"ascii\">0 1 2</DataArray>\n<DataArray type=\"Int64\" "
        "Name=\"offsets\" format=\"ascii\">3</DataArray></Polys>\n" );
    check_load_fails( "zlib_header_overflow.vtp", content );
}

std::string ascii_polys(
    std::string_view connectivity, std::string_view offsets )
{
    return absl::StrCat( "<Polys><DataArray type=\"Int64\" "
                         "Name=\"connectivity\" format=\"ascii\">",
        connectivity,
        "</DataArray>\n<DataArray type=\"Int64\" Name=\"offsets\" "
        "format=\"ascii\">",
        offsets, "</DataArray></Polys>\n" );
}

void test_wrong_number_of_offsets()
{
    check_load_fails( "wrong_number_of_offsets.vtp",
        vtp_file( "", "NumberOfPoints=\"4\" NumberOfPolys=\"2\"",
            absl::StrCat( ascii_points(), ascii_polys( "0 1 2", "3" ) ) ) );
}

void test_invalid_offsets()
{
    check_load_fails( "offsets_out_of_range.vtp",
        vtp_file( "", "NumberOfPoints=\"4\" NumberOfPolys=\"2\"",
            absl::StrCat(
                ascii_points(), ascii_polys( "0 1 2", "3 100000" ) ) ) );
    check_load_fails( "offsets_decreasing.vtp",
        vtp_file( "", "NumberOfPoints=\"4\" NumberOfPolys=\"2\"",
            absl::StrCat(
                ascii_points(), ascii_polys( "0 1 2 1 3 2", "3 2" ) ) ) );
}

std::unique_ptr< geode::PolygonalSurface3D > load_file(
    std::string_view filename, std::string_view content )
{
    write_file( filename, content );
    return geode::load_polygonal_surface< 3 >( filename );
}

bool has_attribute(
    const geode::AttributeManager& manager, std::string_view name )
{
    const auto ids = manager.attribute_ids_matching_name( name );
    return ids && !ids->empty();
}

void test_zero_components()
{
    const auto surface = load_file( "zero_components.vtp",
        vtp_file( "", "NumberOfPoints=\"4\" NumberOfPolys=\"1\"",
            absl::StrCat( ascii_points(), ascii_polys( "0 1 2", "3" ),
                "<PointData><DataArray type=\"Float64\" Name=\"zero\" "
                "NumberOfComponents=\"0\" format=\"ascii\">1 2 3 "
                "4</DataArray><DataArray type=\"Float64\" Name=\"valid\" "
                "format=\"ascii\">1 2 3 4</DataArray></PointData>\n" ) ) );
    geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 1,
        "Surface with zero component array should be loaded" );
    geode::OpenGeodeIOMeshException::test(
        !has_attribute( surface->vertex_attribute_manager(), "zero" ),
        "Array with zero component should be skipped" );
    geode::OpenGeodeIOMeshException::test(
        has_attribute( surface->vertex_attribute_manager(), "valid" ),
        "Valid array should be loaded" );
}

void test_unsupported_array_type()
{
    const auto surface = load_file( "unsupported_array_type.vtp",
        vtp_file( "", "NumberOfPoints=\"4\" NumberOfPolys=\"1\"",
            absl::StrCat( ascii_points(), ascii_polys( "0 1 2", "3" ),
                "<CellData><DataArray type=\"String\" Name=\"names\" "
                "format=\"ascii\">97 0</DataArray><DataArray "
                "type=\"Int32\" Name=\"valid\" format=\"ascii\">"
                "1</DataArray></CellData>\n" ) ) );
    geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 1,
        "Surface with String array should be loaded" );
    geode::OpenGeodeIOMeshException::test(
        !has_attribute( surface->polygon_attribute_manager(), "names" ),
        "String array should be skipped" );
    geode::OpenGeodeIOMeshException::test(
        has_attribute( surface->polygon_attribute_manager(), "valid" ),
        "Valid array should be loaded" );
}

void test_appended_offset_out_of_range()
{
    const auto appended_data =
        "<AppendedData encoding=\"base64\">_CAAAAAAAAAAAAPA/</AppendedData>\n";
    const auto surface = load_file( "appended_offset_out_of_range.vtp",
        absl::StrCat( "<?xml version=\"1.0\"?>\n<VTKFile type=\"PolyData\" "
                      "version=\"1.0\" byte_order=\"LittleEndian\" "
                      "header_type=\"UInt32\">\n<PolyData>\n<Piece "
                      "NumberOfPoints=\"4\" NumberOfPolys=\"1\">\n",
            ascii_points(), ascii_polys( "0 1 2", "3" ),
            "<CellData><DataArray type=\"Float64\" Name=\"valid\" "
            "format=\"appended\" offset=\"0\"/><DataArray "
            "type=\"Float64\" Name=\"invalid\" format=\"appended\" "
            "offset=\"99999\"/></CellData>\n</Piece>\n</PolyData>\n",
            appended_data, "</VTKFile>\n" ) );
    geode::OpenGeodeIOMeshException::test(
        !has_attribute( surface->polygon_attribute_manager(), "invalid" ),
        "Array with invalid appended offset should be skipped" );
    geode::OpenGeodeIOMeshException::test(
        has_attribute( surface->polygon_attribute_manager(), "valid" ),
        "Valid appended array should be loaded" );
    check_load_fails( "appended_points_offset_out_of_range.vtp",
        absl::StrCat( "<?xml version=\"1.0\"?>\n<VTKFile type=\"PolyData\" "
                      "version=\"1.0\" byte_order=\"LittleEndian\" "
                      "header_type=\"UInt32\">\n<PolyData>\n<Piece "
                      "NumberOfPoints=\"4\" NumberOfPolys=\"1\">\n",
            "<Points><DataArray type=\"Float64\" NumberOfComponents=\"3\" "
            "format=\"appended\" offset=\"99999\"/></Points>\n",
            ascii_polys( "0 1 2", "3" ), "</Piece>\n</PolyData>\n",
            appended_data, "</VTKFile>\n" ) );
}

void test_empty_polygon()
{
    const auto surface = load_file( "empty_polygon.vtp",
        vtp_file( "", "NumberOfPoints=\"4\" NumberOfPolys=\"4\"",
            absl::StrCat( ascii_points(),
                ascii_polys( "0 1 2 1 3 1 3 2", "3 3 5 8" ),
                "<CellData><DataArray type=\"Int32\" Name=\"id\" "
                "format=\"ascii\">10 20 30 40</DataArray></CellData>\n" ) ) );
    geode::OpenGeodeIOMeshException::test( surface->nb_polygons() == 2,
        "Empty and degenerated polygons should be skipped" );
    const auto& manager = surface->polygon_attribute_manager();
    const auto attribute = manager.find_generic_attribute(
        manager.attribute_ids_matching_name( "id" )->at( 0 ) );
    geode::OpenGeodeIOMeshException::test(
        attribute->generic_value( 0 ) == 10.f
            && attribute->generic_value( 1 ) == 40.f,
        "Cell data should be transferred to the valid polygons" );
}

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        test_zlib_header_overflow();
        test_invalid_offsets();
        test_wrong_number_of_offsets();
        test_zero_components();
        test_unsupported_array_type();
        test_appended_offset_out_of_range();
        test_empty_polygon();

        std::vector< geode::uuid > first_vertex_attribute_ids;
        std::vector< geode::uuid > first_polygon_attribute_ids;
        run_test( "dfn1_ascii.vtp", { 187, 10 }, first_vertex_attribute_ids,
            first_polygon_attribute_ids );
        std::vector< geode::uuid > second_polygon_attribute_ids;
        run_test( "dfn2_mesh_compressed.vtp", { 33413, 58820 }, {},
            second_polygon_attribute_ids );
        std::vector< geode::uuid > third_polygon_attribute_ids;
        run_test( "dfn2_mesh_append_encoded.vtp", { 33413, 58820 }, {},
            third_polygon_attribute_ids );
        std::vector< geode::uuid > fourth_polygon_attribute_ids;
        run_test( "dfn2_mesh_append_encoded_compressed.vtp", { 33413, 58820 },
            {}, fourth_polygon_attribute_ids );
        std::vector< geode::uuid > raw_polygon_attribute_ids;
        run_test( "dfn2_mesh_append_raw_compressed.vtp", { 33413, 58820 }, {},
            raw_polygon_attribute_ids );
        std::vector< geode::uuid > fifth_vertex_attribute_ids;
        std::vector< geode::uuid > fifth_polygon_attribute_ids;
        run_test( "dfn3.vtp", { 238819, 13032 }, fifth_vertex_attribute_ids,
            fifth_polygon_attribute_ids );

        run_encoding_tests();
        run_attribute_types_test();
        test_array_attributes();
        test_time_series();
        test_write_errors();
        test_duplicated_names();
        test_texture_files();
        test_texture_split_vertices();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
