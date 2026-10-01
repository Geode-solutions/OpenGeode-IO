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
#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/variable_attribute.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/polygonal_surface_builder.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
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

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();

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
        test_time_series();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
