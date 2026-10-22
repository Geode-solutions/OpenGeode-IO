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

#include <fstream>

#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_cat.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/constant_attribute.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>
#include <geode/basic/string.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/edged_curve_builder.hpp>
#include <geode/mesh/builder/surface_mesh_builder.hpp>
#include <geode/mesh/core/edged_curve.hpp>
#include <geode/mesh/core/mesh_factory.hpp>
#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/core/polyhedral_solid.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/mixin/core/corner.hpp>
#include <geode/model/mixin/core/line.hpp>
#include <geode/model/mixin/core/surface.hpp>
#include <geode/model/representation/builder/brep_builder.hpp>
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
        geode::OpenGeodeIOModelException::test(
            brep_description.brep.nb_corners() == brep_description.nb_corners,
            "Number of corners is not correct" );
        geode::OpenGeodeIOModelException::test(
            brep_description.brep.nb_lines() == brep_description.nb_lines,
            "Number of lines is not correct" );
        geode::OpenGeodeIOModelException::test(
            brep_description.brep.nb_surfaces() == brep_description.nb_surfaces,
            "Number of surfaces is not correct" );
        geode::OpenGeodeIOModelException::test(
            brep_description.brep.nb_blocks() == brep_description.nb_blocks,
            "Number of blocks is not correct" );
    }
    void check_corners( const geode::BRep& brep )
    {
        for( const auto& corner : brep.corners() )
        {
            geode::OpenGeodeIOModelException::test(
                corner.mesh().nb_vertices() == 1,
                "Number of vertices in corners should be 1" );
        }
    }
    void check_lines( const geode::BRep& brep )
    {
        for( const auto& line : brep.lines() )
        {
            const auto& mesh = line.mesh();
            geode::OpenGeodeIOModelException::test( mesh.nb_vertices() > 0,
                "Number of vertices in lines should not be null" );
            geode::OpenGeodeIOModelException::test( mesh.nb_edges() > 0,
                "Number of edges in lines should not be null" );
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
            geode::OpenGeodeIOModelException::test( mesh.nb_vertices() > 0,
                "Number of vertices in surfaces should not be null" );
            geode::OpenGeodeIOModelException::test( mesh.nb_polygons() > 0,
                "Number of polygons in surfaces should not be null" );
            geode::OpenGeodeIOModelException::test(
                count_surface_edge_on_border( mesh ) != 0,
                "No polygon adjacency" );
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
            geode::OpenGeodeIOModelException::test( mesh.nb_vertices() > 0,
                "Number of vertices in blocks should not be null" );
            geode::OpenGeodeIOModelException::test( mesh.nb_polyhedra() > 0,
                "Number of polyhedra in blocks should not be null" );
            geode::OpenGeodeIOModelException::test(
                count_polyhedron_facet_on_borders( mesh ) != 0,
                "No polyhedron adjacency" );
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

    struct GidContent
    {
        absl::flat_hash_set< geode::index_t > nodes;
        std::vector< geode::index_t > element_ids;
        std::vector< geode::index_t > element_nodes;
    };

    GidContent read_gid_file( std::string_view filename )
    {
        std::ifstream file{ geode::to_string( filename ) };
        geode::OpenGeodeIOModelException::test(
            file.good(), "Cannot open ", filename );
        GidContent content;
        std::string line;
        bool in_coordinates{ false };
        bool in_elements{ false };
        while( std::getline( file, line ) )
        {
            if( line == "Coordinates" || line == "Elements" )
            {
                in_coordinates = line == "Coordinates";
                in_elements = line == "Elements";
                continue;
            }
            if( geode::string_starts_with( line, "End" )
                || geode::string_starts_with( line, "MESH" ) )
            {
                in_coordinates = false;
                in_elements = false;
                continue;
            }
            const auto tokens = geode::string_split( line );
            if( in_coordinates )
            {
                content.nodes.insert( geode::string_to_index( tokens[0] ) );
            }
            else if( in_elements )
            {
                content.element_ids.push_back(
                    geode::string_to_index( tokens[0] ) );
                // Last value is the material number
                for( const auto t : geode::Range{ 1, tokens.size() - 1 } )
                {
                    content.element_nodes.push_back(
                        geode::string_to_index( tokens[t] ) );
                }
            }
        }
        return content;
    }

    void check_gid_file(
        std::string_view filename, geode::index_t nb_expected_elements )
    {
        const auto content = read_gid_file( filename );
        absl::flat_hash_set< geode::index_t > element_ids{
            content.element_ids.begin(), content.element_ids.end()
        };
        geode::OpenGeodeIOModelException::test(
            content.element_ids.size() == nb_expected_elements,
            "Wrong number of elements in GiD file" );
        geode::OpenGeodeIOModelException::test(
            element_ids.size() == content.element_ids.size(),
            "GiD element ids should be unique" );
        for( const auto node : content.element_nodes )
        {
            geode::OpenGeodeIOModelException::test(
                content.nodes.contains( node ), "GiD element node ", node,
                " should have coordinates" );
        }
    }

    geode::index_t nb_gid_elements( const geode::BRep& brep )
    {
        geode::index_t nb_elements{ 0 };
        for( const auto& block : brep.blocks() )
        {
            nb_elements += block.mesh().nb_polyhedra();
        }
        for( const auto& surface : brep.surfaces() )
        {
            nb_elements += surface.mesh().nb_polygons();
        }
        return nb_elements;
    }

    using test_function = void ( * )( const geode::BRep& );

    void add_material_attributes( const geode::BRep& brep )
    {
        geode::AttributeValues< geode::index_t > default_values;
        default_values.default_value = 1;
        default_values.no_value = geode::NO_ID;
        geode::AttributeProperties properties;
        properties.assignable = false;
        properties.interpolable = true;
        properties.transferable = true;
        for( const auto& block : brep.blocks() )
        {
            const auto& mesh = block.mesh();
            [[maybe_unused]] auto material_attribute =
                mesh.polyhedron_attribute_manager()
                    .create_attribute< geode::ConstantAttribute,
                        geode::index_t >(
                        "material_number", default_values, properties );
        }
        for( const auto& surface : brep.surfaces() )
        {
            const auto& mesh = surface.mesh();
            [[maybe_unused]] auto material_attribute =
                mesh.polygon_attribute_manager()
                    .create_attribute< geode::ConstantAttribute,
                        geode::index_t >(
                        "material_number", default_values, properties );
        }
    }

    void run_test( std::string_view short_filename, test_function test )
    {
        // Load file
        auto brep = geode::load_brep(
            absl::StrCat( geode::DATA_PATH, short_filename, ".og_brep" ) );
        test( brep );
        add_material_attributes( brep );
        const auto filename_gid =
            absl::StrCat( short_filename, "_output.gid_msh" );
        geode::save_brep( brep, filename_gid );
        check_gid_file( absl::StrCat( short_filename, "_output.gid.msh" ),
            nb_gid_elements( brep ) );
    }

    /*!
     * Adds a fracture surface outside of the blocks, bounded by a line whose
     * vertices are registered before the surface ones.
     */
    void add_fracture( geode::BRep& brep )
    {
        geode::BRepBuilder builder{ brep };
        const auto first_unique_vertex = builder.create_unique_vertices( 3 );
        const auto& line = brep.line( builder.add_line() );
        auto line_builder = builder.line_mesh_builder( line );
        line_builder->create_point( geode::Point3D{ { 10, 10, 10 } } );
        line_builder->create_point( geode::Point3D{ { 11, 10, 10 } } );
        line_builder->create_edge( 0, 1 );
        for( const auto v : geode::LRange{ 2 } )
        {
            builder.set_unique_vertex(
                { line.component_id(), v }, first_unique_vertex + v );
        }
        const auto& surface =
            brep.surface( builder.add_surface( geode::MeshFactory::default_impl(
                geode::TriangulatedSurface3D::type_name_static() ) ) );
        auto surface_builder = builder.surface_mesh_builder( surface );
        surface_builder->create_point( geode::Point3D{ { 10, 10, 10 } } );
        surface_builder->create_point( geode::Point3D{ { 11, 10, 10 } } );
        surface_builder->create_point( geode::Point3D{ { 10, 11, 10 } } );
        surface_builder->create_polygon( { 0, 1, 2 } );
        for( const auto v : geode::LRange{ 3 } )
        {
            builder.set_unique_vertex(
                { surface.component_id(), v }, first_unique_vertex + v );
        }
        builder.add_line_surface_boundary_relationship( line, surface );
    }

    void test_wrong_material_type()
    {
        const auto brep =
            geode::load_brep( absl::StrCat( geode::DATA_PATH, "mss.og_brep" ) );
        add_material_attributes( brep );
        geode::AttributeValues< double > default_values;
        default_values.default_value = 1;
        default_values.no_value = -1;
        for( const auto& block : brep.blocks() )
        {
            auto& manager = block.mesh().polyhedron_attribute_manager();
            manager.delete_attribute(
                manager.attribute_ids_matching_name( "material_number" )
                    .value()
                    .at( 0 ) );
            [[maybe_unused]] auto attribute =
                manager.create_attribute< geode::ConstantAttribute, double >(
                    "material_number", default_values,
                    geode::AttributeProperties{} );
        }
        geode::OpenGeodeIOModelException::test(
            !geode::is_brep_saveable( brep, "wrong_type.gid_msh" ),
            "A material number which is not an index_t should not be "
            "saveable" );
    }

    void test_fracture_outside_blocks()
    {
        auto brep =
            geode::load_brep( absl::StrCat( geode::DATA_PATH, "mss.og_brep" ) );
        add_fracture( brep );
        add_material_attributes( brep );
        geode::save_brep( brep, "fracture_output.gid_msh" );
        check_gid_file( "fracture_output.gid.msh", nb_gid_elements( brep ) );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOModelLibrary::initialize();
        run_test( "mss", &test_brep_mss );
        test_fracture_outside_blocks();
        test_wrong_material_type();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
