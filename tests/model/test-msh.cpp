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
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/geometry/point.hpp>

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

    template < typename Range >
    void check_component_vertices( const geode::BRep& brep, Range components )
    {
        for( const auto& component : components )
        {
            absl::flat_hash_set< geode::index_t > unique_vertices;
            for( const auto v : geode::Range{ component.mesh().nb_vertices() } )
            {
                const auto unique_vertex =
                    brep.unique_vertex( { component.component_id(), v } );
                geode::OpenGeodeIOModelException::test(
                    unique_vertex != geode::NO_ID,
                    "Component vertex should have a unique vertex" );
                geode::OpenGeodeIOModelException::test(
                    unique_vertices.insert( unique_vertex ).second,
                    "Component vertices should not be duplicated" );
            }
        }
    }

    void check_unique_vertices( const geode::BRep& brep )
    {
        check_component_vertices( brep, brep.corners() );
        check_component_vertices( brep, brep.lines() );
        check_component_vertices( brep, brep.surfaces() );
        check_component_vertices( brep, brep.blocks() );
    }

    void test_brep( const BrepDescription& brep_description )
    {
        check_unique_vertices( brep_description.brep );
        check_count_components( brep_description );
        check_corners( brep_description.brep );
        check_lines( brep_description.brep );
        check_surfaces( brep_description.brep );
        check_blocks( brep_description.brep );
    }

    void test_brep_cube( const geode::BRep& brep )
    {
        // NOLINTBEGIN(*-magic-numbers)
        BrepDescription brep_cube_description{ brep, 8, 12, 6, 1 };
        // NOLINTEND(*-magic-numbers)
        test_brep( brep_cube_description );

        // Number of component boundaries and incidences
        for( const auto& c : brep.corners() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( c.id() ) == 0,
                "Number of corner boundary should be 0" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( c.id() ) == 3,
                "Number of corner incidences should be 3" );
        }
        for( const auto& l : brep.lines() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( l.id() ) == 2,
                "Number of line boundary should be 2" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( l.id() ) == 2,
                "Number of line incidences should be 2" );
        }
        for( const auto& s : brep.surfaces() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( s.id() ) == 4,
                "Number of surface boundary should be 4" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( s.id() ) == 1,
                "Number of surface incidences should be 1" );
        }
        for( const auto& b : brep.blocks() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( b.id() ) == 6,
                "Number of block boundary should be 6" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( b.id() ) == 0,
                "Number of block incidences should be 0" );
        }
    }

    void test_brep_cone( const geode::BRep& brep )
    {
        // NOLINTBEGIN(*-magic-numbers)
        BrepDescription brep_cone_description{ brep, 6, 13, 12, 4 };
        // NOLINTEND(*-magic-numbers)
        test_brep( brep_cone_description );

        // Number of component boundaries and incidences
        for( const auto& c : brep.corners() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( c.id() ) == 0,
                "Number of corner boundary should be 0" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( c.id() ) == 4
                    || brep.nb_incidences( c.id() ) == 5,
                "Number of corner incidences should be 4 or 5" );
        }
        for( const auto& l : brep.lines() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( l.id() ) == 2,
                "Number of line boundary should be 2" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( l.id() ) > 1
                    && brep.nb_incidences( l.id() ) < 5,
                "Number of line incidences should be 2, 3 or 4" );
        }
        for( const auto& s : brep.surfaces() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( s.id() ) == 3,
                "Number of surface boundary should be 3" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( s.id() ) == 1
                    || brep.nb_incidences( s.id() ) == 2,
                "Number of surface incidences should be 1 or 2" );
        }
        for( const auto& b : brep.blocks() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( b.id() ) == 4,
                "Number of block boundary should be 4" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( b.id() ) == 0,
                "Number of block incidences should be 0" );
        }
    }

    void test_brep_internal( const geode::BRep& brep )
    {
        // NOLINTBEGIN(*-magic-numbers)
        BrepDescription brep_internal_description{ brep, 5, 4, 1, 0 };
        // NOLINTEND(*-magic-numbers)
        test_brep( brep_internal_description );

        // Number of component boundaries and incidences
        for( const auto& c : brep.corners() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( c.id() ) == 0,
                "Number of corner boundary should be 0" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( c.id() ) == 1
                    || brep.nb_incidences( c.id() ) == 2,
                "Number of corner incidences should be 1 or 2" );
        }
        for( const auto& l : brep.lines() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( l.id() ) == 2,
                "Number of line boundary should be 2" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( l.id() ) == 1
                    || brep.nb_embedding_surfaces( l ) == 1,
                "Number of line incidences should be 1 or embedding in "
                "1 "
                "surface" );
        }
        for( const auto& s : brep.surfaces() )
        {
            geode::OpenGeodeIOModelException::test(
                brep.nb_boundaries( s.id() ) == 3,
                "Number of surface boundary should be 3" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_internal_lines( s ) == 1,
                "Number of surface internal should be 1" );
            geode::OpenGeodeIOModelException::test(
                brep.nb_incidences( s.id() ) == 0,
                "Number of surface incidences should be 0" );
        }
    }

    void write_file( std::string_view filename, std::string_view content )
    {
        std::ofstream file{ geode::to_string( filename ) };
        file << content;
    }

    constexpr auto V2_HEADER = "$MeshFormat\n2.2 0 8\n$EndMeshFormat\n";

    constexpr auto V2_TRIANGLE_NODES = "$Nodes\n3\n"
                                       "1 0 0 0\n"
                                       "2 1 0 0\n"
                                       "3 0 1 0\n"
                                       "$EndNodes\n";

    constexpr auto V4_HEADER = "$MeshFormat\n4.1 0 8\n$EndMeshFormat\n";

    constexpr auto V4_TRIANGLE_NODES = "$Nodes\n1 3 1 3\n2 1 0 3\n1\n2\n3\n"
                                       "0 0 0\n"
                                       "1 0 0\n"
                                       "0 1 0\n"
                                       "$EndNodes\n";

    const geode::SurfaceMesh3D& first_surface_mesh( const geode::BRep& brep )
    {
        for( const auto& surface : brep.surfaces() )
        {
            return surface.mesh();
        }
        throw geode::OpenGeodeIOModelException{ nullptr,
            geode::OpenGeodeException::TYPE::internal, "No Surface in BRep" };
    }

    void test_load_failure(
        std::string_view filename, std::string_view content )
    {
        write_file( filename, content );
        try
        {
            const auto brep = geode::load_brep( filename );
        }
        catch( const geode::OpenGeodeException& exception )
        {
            geode::OpenGeodeIOModelException::test(
                exception.type() == geode::OpenGeodeException::TYPE::data,
                "Loading ", filename,
                " should fail with a data error, not an internal one" );
            return;
        }
        throw geode::OpenGeodeIOModelException{ nullptr,
            geode::OpenGeodeException::TYPE::internal, "Loading ", filename,
            " should fail" };
    }

    void test_truncated_lines()
    {
        test_load_failure(
            "truncated_header.msh", "$MeshFormat\n2.2\n$EndMeshFormat\n" );
        test_load_failure( "truncated_node.msh",
            absl::StrCat( V2_HEADER, "$Nodes\n1\n1 0 0\n$EndNodes\n" ) );
        test_load_failure( "truncated_file.msh",
            absl::StrCat( V2_HEADER, "$Nodes\n3\n1 0 0 0\n" ) );
        test_load_failure( "truncated_element_v2.msh",
            absl::StrCat( V2_HEADER, V2_TRIANGLE_NODES,
                "$Elements\n1\n1 2 2 0 1 1 2\n$EndElements\n" ) );
        test_load_failure( "too_many_tags_v2.msh",
            absl::StrCat( V2_HEADER, V2_TRIANGLE_NODES,
                "$Elements\n1\n1 2 4294967295 0 1 1 2 3\n$EndElements\n" ) );
        test_load_failure( "truncated_element_v4.msh",
            absl::StrCat( V4_HEADER, V4_TRIANGLE_NODES,
                "$Elements\n1 1 1 1\n2 1 2 1\n1 1 2\n$EndElements\n" ) );
        test_load_failure( "empty_element_v4.msh",
            absl::StrCat( V4_HEADER, V4_TRIANGLE_NODES,
                "$Elements\n1 1 1 1\n2 1 2 1\n\n$EndElements\n" ) );
    }

    void test_node_tags()
    {
        test_load_failure( "unknown_node_v2.msh",
            absl::StrCat( V2_HEADER, V2_TRIANGLE_NODES,
                "$Elements\n1\n1 2 2 0 1 1 2 4\n$EndElements\n" ) );
        test_load_failure( "null_node_v2.msh",
            absl::StrCat( V2_HEADER, V2_TRIANGLE_NODES,
                "$Elements\n1\n1 2 2 0 1 0 2 3\n$EndElements\n" ) );
        test_load_failure( "duplicated_node_v2.msh",
            absl::StrCat( V2_HEADER,
                "$Nodes\n3\n1 0 0 0\n2 1 0 0\n2 0 1 0\n$EndNodes\n",
                "$Elements\n1\n1 2 2 0 1 1 2 3\n$EndElements\n" ) );
        test_load_failure( "unknown_node_v4.msh",
            absl::StrCat( V4_HEADER, V4_TRIANGLE_NODES,
                "$Elements\n1 1 1 1\n2 1 2 1\n1 1 2 7\n$EndElements\n" ) );
        test_load_failure( "node_out_of_range_v2.msh",
            absl::StrCat( V2_HEADER,
                "$Nodes\n3\n1 0 0 0\n2 1 0 0\n1000000 0 1 0\n$EndNodes\n",
                "$Elements\n1\n1 2 2 0 1 1 2 3\n$EndElements\n" ) );

        write_file( "sparse_nodes_v2.msh",
            absl::StrCat( V2_HEADER,
                "$Nodes\n3\n10 0 0 0\n5 1 0 0\n1000000 0 1 0\n$EndNodes\n",
                "$Elements\n1\n1 2 2 0 1 10 5 1000000\n$EndElements\n" ) );
        const auto brep = geode::load_brep( "sparse_nodes_v2.msh" );
        geode::OpenGeodeIOModelException::test( brep.nb_unique_vertices() == 3,
            "Sparse node tags should give 3 unique vertices" );
        const auto& mesh = first_surface_mesh( brep );
        geode::OpenGeodeIOModelException::test(
            mesh.point( mesh.polygon_vertex( { 0, 2 } ) )
                == geode::Point3D{ { 0, 1, 0 } },
            "Sparse node tags should be mapped to the right coordinates" );
    }

    void test_sparse_element_tags()
    {
        write_file( "sparse_elements_v2.msh",
            absl::StrCat( V2_HEADER,
                "$Nodes\n4\n1 0 0 0\n2 1 0 0\n3 0 1 0\n4 1 1 0\n"
                "$EndNodes\n",
                "$Elements\n2\n5 2 2 0 1 1 2 3\n42 2 2 0 1 2 4 3\n"
                "$EndElements\n" ) );
        const auto brep_v2 = geode::load_brep( "sparse_elements_v2.msh" );
        geode::OpenGeodeIOModelException::test(
            first_surface_mesh( brep_v2 ).nb_polygons() == 2,
            "Sparse element tags (v2) should give 2 polygons" );

        write_file( "sparse_elements_v4.msh",
            absl::StrCat( V4_HEADER,
                "$Nodes\n1 4 1 4\n2 1 0 4\n1\n2\n3\n4\n"
                "0 0 0\n1 0 0\n0 1 0\n1 1 0\n$EndNodes\n",
                "$Elements\n1 2 7 12\n2 1 2 2\n7 1 2 3\n12 2 4 3\n"
                "$EndElements\n" ) );
        const auto brep_v4 = geode::load_brep( "sparse_elements_v4.msh" );
        geode::OpenGeodeIOModelException::test(
            first_surface_mesh( brep_v4 ).nb_polygons() == 2,
            "Sparse element tags (v4) should give 2 polygons" );
    }

    void test_parametric_nodes()
    {
        write_file( "parametric_nodes_v4.msh",
            absl::StrCat( V4_HEADER,
                "$Nodes\n1 3 1 3\n2 1 1 3\n1\n2\n3\n"
                "0 0 0 0.1 0.2\n1 0 0 0.3 0.4\n0 1 0 0.5 0.6\n$EndNodes\n",
                "$Elements\n1 1 1 1\n2 1 2 1\n1 1 2 3\n$EndElements\n" ) );
        const auto brep = geode::load_brep( "parametric_nodes_v4.msh" );
        const auto& mesh = first_surface_mesh( brep );
        geode::OpenGeodeIOModelException::test(
            mesh.point( mesh.polygon_vertex( { 0, 1 } ) )
                == geode::Point3D{ { 1, 0, 0 } },
            "Parametric coordinates should be ignored" );
    }

    void test_corners_without_point_elements()
    {
        const auto brep = geode::load_brep( absl::StrCat(
            geode::DATA_PATH, "cube_no_point_elements_v41.msh" ) );
        test_brep_cube( brep );
        for( const auto& corner : brep.corners() )
        {
            const auto& point = corner.mesh().point( 0 );
            for( const auto d : geode::LRange{ 3 } )
            {
                geode::OpenGeodeIOModelException::test(
                    point.value( d ) == 0 || point.value( d ) == 1,
                    "Corner should be on a cube vertex" );
            }
        }
    }

    void test_empty_components()
    {
        write_file( "empty_components_v4.msh",
            absl::StrCat( V4_HEADER,
                "$Entities\n1 1 1 0\n"
                "1 5 5 5 0\n"
                "1 0 0 0 1 0 0 0 0\n"
                "1 0 0 0 1 1 0 0 0\n"
                "$EndEntities\n",
                V4_TRIANGLE_NODES,
                "$Elements\n1 1 1 1\n2 1 2 1\n1 1 2 3\n$EndElements\n" ) );
        const auto brep = geode::load_brep( "empty_components_v4.msh" );
        geode::OpenGeodeIOModelException::test( brep.nb_corners() == 0,
            "Corner without element should be removed" );
        geode::OpenGeodeIOModelException::test(
            brep.nb_lines() == 0, "Line without element should be removed" );
        geode::OpenGeodeIOModelException::test(
            brep.nb_surfaces() == 1, "Surface with elements should be kept" );
    }

    void test_corner_with_several_nodes()
    {
        write_file( "corner_several_nodes_v2.msh",
            absl::StrCat( V2_HEADER, V2_TRIANGLE_NODES,
                "$Elements\n3\n1 15 2 0 1 1\n2 15 2 0 1 2\n"
                "3 2 2 0 1 1 2 3\n$EndElements\n" ) );
        const auto brep = geode::load_brep( "corner_several_nodes_v2.msh" );
        for( const auto& corner : brep.corners() )
        {
            geode::OpenGeodeIOModelException::test(
                corner.mesh().nb_vertices() == 1,
                "Corner should have only one vertex" );
        }
    }

    void test_unknown_bounding_entities()
    {
        // Gmsh writes an invalid bounding curve tag for the curve embedded in
        // the bottom surface
        const auto brep = geode::load_brep(
            absl::StrCat( geode::DATA_PATH, "embedded_v41.msh" ) );
        // NOLINTBEGIN(*-magic-numbers)
        BrepDescription description{ brep, 14, 17, 7, 1 };
        // NOLINTEND(*-magic-numbers)
        check_count_components( description );
        check_unique_vertices( brep );
    }

    void test_is_loadable()
    {
        write_file( "unsupported_version.msh",
            "$MeshFormat\n3.0 0 8\n$EndMeshFormat\n" );
        geode::OpenGeodeIOModelException::test(
            geode::is_brep_loadable( "unsupported_version.msh" ).value() == 0,
            "MSH file with unsupported version should not be loadable" );
        write_file( "header_only.msh", V2_HEADER );
        geode::OpenGeodeIOModelException::test(
            geode::is_brep_loadable( "header_only.msh" ).value() == 1,
            "MSH file with a supported header should be loadable" );
    }

    void test_skip_unknown_sections()
    {
        write_file( "unknown_sections.msh",
            absl::StrCat( V2_HEADER,
                "$Comments\n$Nodes are described below\n$EndComments\n",
                V2_TRIANGLE_NODES,
                "$Elements\n1\n1 2 2 0 1 1 2 3\n$EndElements\n",
                "$NodeData\n1\n\"data\"\n$EndNodeData\n" ) );
        const auto brep = geode::load_brep( "unknown_sections.msh" );
        geode::OpenGeodeIOModelException::test(
            brep.nb_surfaces() == 1, "Unknown sections should be skipped" );
        geode::OpenGeodeIOModelException::test( brep.nb_unique_vertices() == 3,
            "Nodes should be read from the $Nodes section" );
    }

    using test_function = void ( * )( const geode::BRep& );

    void run_test( std::string_view short_filename, test_function test )
    {
        // Load file
        auto brep = geode::load_brep(
            absl::StrCat( geode::DATA_PATH, short_filename, ".msh" ) );
        test( brep );

        // Save and reload
        const auto filename =
            absl::StrCat( short_filename, ".", brep.native_extension() );
        geode::save_brep( brep, filename );
        auto reloaded_brep = geode::load_brep( filename );
        test( reloaded_brep );

        const auto filename_msh = absl::StrCat( short_filename, "_output.msh" );
        geode::save_brep( brep, filename_msh );
        auto reloaded_brep2 = geode::load_brep( filename_msh );
        test( reloaded_brep2 );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOModelLibrary::initialize();

        run_test( "triangle_internal", &test_brep_internal );
        run_test( "cube_v22", &test_brep_cube );
        run_test( "cone_v4", &test_brep_cone );
        run_test( "cube_v40", &test_brep_cube );

        test_is_loadable();
        test_skip_unknown_sections();
        test_truncated_lines();
        test_node_tags();
        test_sparse_element_tags();
        test_parametric_nodes();
        test_corners_without_point_elements();
        test_empty_components();
        test_corner_with_several_nodes();
        test_unknown_bounding_entities();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
