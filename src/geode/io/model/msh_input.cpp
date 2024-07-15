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

#include <geode/io/model/internal/msh_input.h>

#include <fstream>
#include <mutex>
#include <numeric>

#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_split.h>

#include <geode/basic/algorithm.h>
#include <geode/basic/common.h>
#include <geode/basic/factory.h>
#include <geode/basic/logger.h>
#include <geode/basic/string.h>
#include <geode/basic/uuid.h>

#include <geode/geometry/point.h>

#include <geode/mesh/builder/edged_curve_builder.h>
#include <geode/mesh/builder/point_set_builder.h>
#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/builder/polyhedral_solid_builder.h>
#include <geode/mesh/core/edged_curve.h>
#include <geode/mesh/core/hybrid_solid.h>
#include <geode/mesh/core/mesh_factory.h>
#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/polygonal_surface.h>

#include <geode/model/helpers/detail/build_model_boundaries.h>
#include <geode/model/mixin/core/block.h>
#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/mixin/core/surface.h>
#include <geode/model/representation/builder/brep_builder.h>
#include <geode/model/representation/core/brep.h>

#include <geode/io/model/internal/msh_common.h>

namespace
{
    class MSHInputImpl
    {
    public:
        MSHInputImpl( std::string_view filename, geode::BRep& brep )
            : file_{ geode::to_string( filename ) },
              brep_( brep ),
              builder_{ brep }
        {
            OPENGEODE_EXCEPTION( file_.good(),
                "[MSHInput] Error while opening file: ", filename );
            first_read( filename );
        }

        void read_file()
        {
            if( version() == 4
                && ( absl::c_find( sections_, "$Entities" )
                     != sections_.end() ) )
            {
                read_entity_section();
            }
            if( version() == 2 )
            {
                read_node_section_v2();
                read_element_section_v2();
            }
            else if( version() == 4 )
            {
                read_node_section_v4();
                read_element_section_v4();
            }
            else
            {
                OPENGEODE_ASSERT_NOT_REACHED(
                    "[MSHInput::read_file] Only MSH file format "
                    "versions 2 and 4 are supported for now." );
            }
        }

        void build_geometry()
        {
            build_corners();
            build_lines();
            build_surfaces();
            build_blocks();
        }

        using boundary_incidences_relations = absl::flat_hash_map< geode::uuid,
            absl::flat_hash_set< geode::uuid > >;

        void build_topology()
        {
            if( version() == 4
                && ( absl::c_find( sections_, "$Entities" )
                     != sections_.end() ) )
            {
                return;
            }
            boundary_incidences_relations corner_line_relations;
            boundary_incidences_relations line_surface_relations;
            boundary_incidences_relations surface_block_relations;
            for( const auto uv : geode::Range{ brep_.nb_unique_vertices() } )
            {
                const auto cmv = get_component_mesh_vertices( uv );
                add_potential_relationships( std::get< 0 >( cmv ),
                    std::get< 1 >( cmv ), corner_line_relations );
                add_potential_relationships( std::get< 1 >( cmv ),
                    std::get< 2 >( cmv ), line_surface_relations );
                add_potential_relationships( std::get< 2 >( cmv ),
                    std::get< 3 >( cmv ), surface_block_relations );
            }
            for( const auto uv : geode::Range{ brep_.nb_unique_vertices() } )
            {
                std::vector< geode::ComponentMeshVertex > corners_vertices;
                std::vector< geode::ComponentMeshVertex > lines_vertices;
                std::vector< geode::ComponentMeshVertex > surfaces_vertices;
                std::vector< geode::ComponentMeshVertex > blocks_vertices;
                const auto cmv = brep_.component_mesh_vertices( uv );
                for( const auto v : cmv )
                {
                    if( v.component_id.type()
                        == geode::Corner3D::component_type_static() )
                    {
                        corners_vertices.push_back( v );
                        continue;
                    }
                    if( v.component_id.type()
                        == geode::Line3D::component_type_static() )
                    {
                        lines_vertices.push_back( v );
                        continue;
                    }
                    if( v.component_id.type()
                        == geode::Surface3D::component_type_static() )
                    {
                        surfaces_vertices.push_back( v );
                        continue;
                    }
                    if( v.component_id.type()
                        == geode::Block3D::component_type_static() )
                    {
                        blocks_vertices.push_back( v );
                        continue;
                    }
                }
                filter_potential_relationships(
                    corners_vertices, lines_vertices, corner_line_relations );
                filter_potential_relationships(
                    lines_vertices, surfaces_vertices, line_surface_relations );
                filter_potential_relationships( surfaces_vertices,
                    blocks_vertices, surface_block_relations );
            }

            for( const auto& c2l : corner_line_relations )
            {
                for( const auto& line_id : c2l.second )
                {
                    builder_.add_corner_line_boundary_relationship(
                        brep_.corner( c2l.first ), brep_.line( line_id ) );
                }
            }
            for( const auto& l2s : line_surface_relations )
            {
                for( const auto& surface_id : l2s.second )
                {
                    builder_.add_line_surface_boundary_relationship(
                        brep_.line( l2s.first ), brep_.surface( surface_id ) );
                }
            }
            for( const auto& s2b : surface_block_relations )
            {
                for( const auto& block_id : s2b.second )
                {
                    builder_.add_surface_block_boundary_relationship(
                        brep_.surface( s2b.first ), brep_.block( block_id ) );
                }
            }
            geode::detail::build_model_boundaries( brep_, builder_ );
        }

    private:
        geode::index_t version() const
        {
            return static_cast< geode::index_t >( std::floor( version_ ) );
        }

        void first_read( std::string_view filename )
        {
            std::ifstream reader{ geode::to_string( filename ) };
            read_header( reader );
        }

        void check_keyword( const std::string& keyword )
        {
            check_keyword( file_, keyword );
        }

        void check_keyword( std::ifstream& reader, const std::string& keyword )
        {
            std::string line;
            std::getline( reader, line );
            OPENGEODE_EXCEPTION( geode::string_starts_with( line, keyword ),
                "[MSHInput::check_keyword] Line should starts with \"", keyword,
                "\"" );
        }

        void go_to_section( const std::string& section_header )
        {
            std::string line;
            while( std::getline( file_, line ) )
            {
                if( geode::string_starts_with( line, section_header ) )
                {
                    return;
                }
            }
            throw geode::OpenGeodeException{
                "[MSHInput::go_to_section] Cannot find the section "
                + section_header
            };
        }

        void set_msh_version( const std::string& line )
        {
            const auto header_tokens = geode::string_split( line );
            const auto ok = absl::SimpleAtod( header_tokens[0], &version_ );
            OPENGEODE_EXCEPTION( ok, "[MSHInput::set_msh_version] Error while "
                                     "reading file version" );
            OPENGEODE_EXCEPTION( version() == 2 || version() == 4,
                "[MSHInput::set_msh_version] Only MSH file format "
                "versions 2 and 4 are supported for now." );
            if( geode::string_to_index( header_tokens[1] ) != 0 )
            {
                binary_ = false;
                throw geode::OpenGeodeException{ "[MSHInput::set_msh_version]"
                                                 " Binary format is not "
                                                 "supported for now." };
            }
        }

        void read_header( std::ifstream& reader )
        {
            check_keyword( reader, "$MeshFormat" );
            std::string line;
            std::getline( reader, line );
            set_msh_version( line );
            check_keyword( reader, "$EndMeshFormat" );
            read_section_names( reader );
        }

        void read_section_names( std::ifstream& reader )
        {
            std::string line;
            while( std::getline( reader, line ) )
            {
                if( geode::string_starts_with( line, "$" )
                    && !geode::string_starts_with( line, "$End" ) )
                {
                    absl::RemoveExtraAsciiWhitespace( &line );
                    sections_.push_back( line );
                }
            }
        }

        using MshId2Uuid = absl::flat_hash_map< geode::index_t, geode::uuid >;

        void read_entity_section()
        {
            go_to_section( "$Entities" );
            std::string line;
            std::getline( file_, line );
            const auto tokens = geode::string_split( line );
            create_corners( geode::string_to_index( tokens.at( 0 ) ) );
            create_lines( geode::string_to_index( tokens.at( 1 ) ) );
            create_surfaces( geode::string_to_index( tokens.at( 2 ) ) );
            create_blocks( geode::string_to_index( tokens.at( 3 ) ) );
            check_keyword( "$EndEntities" );
        }

        void create_corners( const geode::index_t nb_corners )
        {
            for( const auto unused : geode::Range{ nb_corners } )
            {
                geode_unused( unused );
                std::string line;
                std::getline( file_, line );
                const auto tokens = geode::string_split( line );
                const auto corner_uuid = builder_.add_corner();
                gmsh_id2uuids_
                    .elementary_ids[{ geode::Corner3D::component_type_static(),
                        geode::string_to_index( tokens.at( 0 ) ) }] =
                    corner_uuid;
            }
        }

        void create_lines( const geode::index_t nb_lines )
        {
            for( const auto unused : geode::Range{ nb_lines } )
            {
                geode_unused( unused );
                std::string line;
                std::getline( file_, line );
                const auto tokens = geode::string_split( line );
                const auto line_uuid = builder_.add_line();
                gmsh_id2uuids_
                    .elementary_ids[{ geode::Line3D::component_type_static(),
                        geode::string_to_index( tokens.at( 0 ) ) }] = line_uuid;
                // TODO physical tags
                const auto nb_physical_tags =
                    geode::string_to_index( tokens.at( 7 ) );
                for( const auto b : geode::Range{ geode::string_to_index(
                         tokens.at( 8 + nb_physical_tags ) ) } )
                {
                    geode::signed_index_t boundary_msh_id;
                    const auto ok =
                        absl::SimpleAtoi( tokens.at( 9 + nb_physical_tags + b ),
                            &boundary_msh_id );
                    OPENGEODE_EXCEPTION( ok,
                        "[MSHInput::create_lines] "
                        "Error while reading boundary entity index" );
                    boundary_msh_id = std::abs( boundary_msh_id );
                    builder_.add_corner_line_boundary_relationship(
                        brep_.corner( gmsh_id2uuids_.elementary_ids.at(
                            { geode::Corner3D::component_type_static(),
                                static_cast< geode::index_t >(
                                    boundary_msh_id ) } ) ),
                        brep_.line( line_uuid ) );
                }
            }
        }

        void create_surfaces( const geode::index_t nb_surfaces )
        {
            for( const auto unused : geode::Range{ nb_surfaces } )
            {
                geode_unused( unused );
                std::string file_line;
                std::getline( file_, file_line );
                const auto tokens = geode::string_split( file_line );
                const auto surface_uuid = builder_.add_surface();
                const auto& surface = brep_.surface( surface_uuid );
                gmsh_id2uuids_
                    .elementary_ids[{ geode::Surface3D::component_type_static(),
                        geode::string_to_index( tokens.at( 0 ) ) }] =
                    surface_uuid;
                // TODO physical tags
                absl::flat_hash_map< geode::index_t, geode::index_t >
                    boundary_counter;
                const auto nb_physical_tags =
                    geode::string_to_index( tokens.at( 7 ) );
                for( const auto b : geode::Range{ geode::string_to_index(
                         tokens.at( 8 + nb_physical_tags ) ) } )
                {
                    geode::signed_index_t boundary_msh_id;
                    const auto ok =
                        absl::SimpleAtoi( tokens.at( 9 + nb_physical_tags + b ),
                            &boundary_msh_id );
                    OPENGEODE_EXCEPTION( ok,
                        "[MSHInput::create_surfaces] "
                        "Error while reading boundary entity index" );
                    auto it = boundary_counter.emplace(
                        static_cast< geode::index_t >(
                            std::abs( boundary_msh_id ) ),
                        1 );
                    if( !it.second )
                    {
                        it.first->second++;
                    }
                }
                for( const auto& boundary : boundary_counter )
                {
                    const auto& line =
                        brep_.line( gmsh_id2uuids_.elementary_ids.at(
                            { geode::Line3D::component_type_static(),
                                boundary.first } ) );
                    if( boundary.second == 1 )
                    {
                        builder_.add_line_surface_boundary_relationship(
                            line, surface );
                    }
                    else
                    {
                        OPENGEODE_ASSERT( boundary.second == 2,
                            "[MSHInput::create_surfaces] Wrong Surface/Line "
                            "relationship" );
                        builder_.add_line_surface_internal_relationship(
                            line, surface );
                    }
                }
            }
        }

        void create_blocks( const geode::index_t nb_blocks )
        {
            for( const auto unused : geode::Range{ nb_blocks } )
            {
                geode_unused( unused );
                std::string line;
                std::getline( file_, line );
                const auto tokens = geode::string_split( line );

                const auto block_uuid =
                    builder_.add_block( geode::MeshFactory::default_impl(
                        geode::HybridSolid3D::type_name_static() ) );
                gmsh_id2uuids_
                    .elementary_ids[{ geode::Block3D::component_type_static(),
                        geode::string_to_index( tokens.at( 0 ) ) }] =
                    block_uuid;
                // TODO physical tags
                const auto nb_physical_tags =
                    geode::string_to_index( tokens.at( 7 ) );
                for( const auto b : geode::Range{ geode::string_to_index(
                         tokens.at( 8 + nb_physical_tags ) ) } )
                {
                    geode::signed_index_t boundary_msh_id;
                    const auto ok =
                        absl::SimpleAtoi( tokens.at( 9 + nb_physical_tags + b ),
                            &boundary_msh_id );
                    OPENGEODE_EXCEPTION( ok,
                        "[MSHInput::create_blocks] "
                        "Error while reading boundary entity index" );
                    boundary_msh_id = std::abs( boundary_msh_id );
                    builder_.add_surface_block_boundary_relationship(
                        brep_.surface( gmsh_id2uuids_.elementary_ids.at(
                            { geode::Surface3D::component_type_static(),
                                static_cast< geode::index_t >(
                                    boundary_msh_id ) } ) ),
                        brep_.block( block_uuid ) );
                }
            }
        }

        geode::Point3D read_node_coordinates( std::string_view x_str,
            std::string_view y_str,
            std::string_view z_str )
        {
            double x, y, z;
            auto ok = absl::SimpleAtod( x_str, &x );
            OPENGEODE_EXCEPTION( ok, "[MSHInput::read_node_coordinates] "
                                     "Error while reading node coordinates" );
            ok = absl::SimpleAtod( y_str, &y );
            OPENGEODE_EXCEPTION( ok, "[MSHInput::read_node_coordinates] "
                                     "Error while reading node coordinates" );
            ok = absl::SimpleAtod( z_str, &z );
            OPENGEODE_EXCEPTION( ok, "[MSHInput::read_node_coordinates] "
                                     "Error while reading node coordinates" );
            return geode::Point3D{ { x, y, z } };
        }

        void read_node_section_v2()
        {
            go_to_section( "$Nodes" );
            std::string line;
            std::getline( file_, line );
            const auto tokens = geode::string_split( line );
            const auto nb_nodes = geode::string_to_index( tokens.at( 0 ) );
            nodes_.resize( nb_nodes );
            for( const auto unused : geode::Range{ nb_nodes } )
            {
                geode_unused( unused );
                std::getline( file_, line );
                geode::index_t node_id;
                geode::Point3D node;
                std::tie( node_id, node ) = read_node( line );
                nodes_[node_id - geode::internal::GMSH_OFFSET_START] = node;
            }
            check_keyword( "$EndNodes" );
            builder_.create_unique_vertices( nb_nodes );
        }

        std::tuple< geode::index_t, geode::Point3D > read_node(
            const std::string& line )
        {
            const auto tokens = geode::string_split( line );
            return std::make_tuple( geode::string_to_index( tokens.at( 0 ) ),
                read_node_coordinates(
                    tokens.at( 1 ), tokens.at( 2 ), tokens.at( 3 ) ) );
        }

        void read_node_section_v4()
        {
            go_to_section( "$Nodes" );
            std::string line;
            std::getline( file_, line );
            const auto tokens = geode::string_split( line );
            const auto nb_total_nodes =
                geode::string_to_index( tokens.at( 1 ) );
            const auto min_node_id = geode::string_to_index( tokens.at( 2 ) );
            const auto max_node_id = geode::string_to_index( tokens.at( 3 ) );
            OPENGEODE_EXCEPTION(
                min_node_id == 1 && max_node_id == nb_total_nodes,
                "[MSHInput::read_node_section_v4] Non continuous node indexing "
                "is not supported for now" );
            nodes_.resize( nb_total_nodes );
            for( const auto unused :
                geode::Range{ geode::string_to_index( tokens.at( 0 ) ) } )
            {
                geode_unused( unused );
                read_node_group();
            }
            check_keyword( "$EndNodes" );
            builder_.create_unique_vertices( nb_total_nodes );
        }

        void read_node_group()
        {
            std::string line;
            std::getline( file_, line );
            const auto tokens = geode::string_split( line );
            const auto nb_nodes = geode::string_to_index( tokens.at( 3 ) );
            OPENGEODE_EXCEPTION( geode::string_to_index( tokens.at( 2 ) ) == 0,
                "[MSHInput::read_node_group] Parametric node coordinates "
                "is not supported for now" );
            absl::FixedArray< geode::index_t > node_ids( nb_nodes );
            for( const auto n : geode::Range{ nb_nodes } )
            {
                std::getline( file_, line );
                const auto node_tokens = geode::string_split( line );
                node_ids[n] = geode::string_to_index( node_tokens.at( 0 ) );
            }
            for( const auto node_id : node_ids )
            {
                std::getline( file_, line );
                const auto node_tokens = geode::string_split( line );
                nodes_[node_id - geode::internal::GMSH_OFFSET_START] =
                    read_node_coordinates( node_tokens.at( 0 ),
                        node_tokens.at( 1 ), node_tokens.at( 2 ) );
            }
        }

        void read_element_section_v2()
        {
            go_to_section( "$Elements" );
            std::string line;
            std::getline( file_, line );
            const auto tokens = geode::string_split( line );
            const auto nb_elements = geode::string_to_index( tokens.at( 0 ) );
            for( auto e_id : geode::Range{ nb_elements } )
            {
                std::getline( file_, line );
                read_element( e_id + geode::internal::GMSH_OFFSET_START, line );
            }
            check_keyword( "$EndElements" );
        }

        void read_element(
            geode::index_t expected_element_id, const std::string& line )
        {
            const auto tokens = geode::string_split( line );
            geode::index_t t{ 0 };
            OPENGEODE_EXCEPTION(
                expected_element_id
                    == geode::string_to_index( tokens.at( t++ ) ),
                "[MSHInput::read_element] Element indices should be "
                "continuous." );

            // Element type
            const auto mesh_element_type_id =
                geode::string_to_index( tokens.at( t++ ) );
            // Tags
            const auto nb_tags = geode::string_to_index( tokens.at( t++ ) );
            OPENGEODE_EXCEPTION( nb_tags >= 2, "[MSHInput::read_element] "
                                               "Number of tags for an element "
                                               "should be at least 2." );
            const auto physical_entity =
                geode::string_to_index( tokens.at( t++ ) );
            const auto elementary_entity =
                geode::string_to_index( tokens.at( t++ ) );
            t += nb_tags - 2;
            // TODO: create relation to the parent
            absl::Span< const std::string_view > vertex_ids(
                &tokens[t], tokens.size() - t );

            const auto element = geode::internal::GMSHElementFactory::create(
                mesh_element_type_id, physical_entity, elementary_entity,
                vertex_ids );
            element->add_element( brep_, gmsh_id2uuids_ );
        }

        void read_element_section_v4()
        {
            go_to_section( "$Elements" );
            std::string line;
            std::getline( file_, line );
            const auto tokens = geode::string_split( line );
            const auto nb_total_elements =
                geode::string_to_index( tokens.at( 1 ) );
            const auto min_element_id =
                geode::string_to_index( tokens.at( 2 ) );
            const auto max_element_id =
                geode::string_to_index( tokens.at( 3 ) );
            OPENGEODE_EXCEPTION(
                min_element_id == 1 && max_element_id == nb_total_elements,
                "[MSHInput::read_element_section_v4] Non continuous element "
                "indexing is not supported for now" );
            for( const auto unused :
                geode::Range{ geode::string_to_index( tokens.at( 0 ) ) } )
            {
                geode_unused( unused );
                read_element_group();
            }
            check_keyword( "$EndElements" );
        }

        void read_element_group()
        {
            std::string line;
            std::getline( file_, line );
            const auto tokens = geode::string_split( line );
            const auto entity_id = geode::string_to_index( tokens.at( 1 ) );
            const auto mesh_element_type_id =
                geode::string_to_index( tokens.at( 2 ) );
            for( const auto unused :
                geode::Range{ geode::string_to_index( tokens.at( 3 ) ) } )
            {
                geode_unused( unused );
                std::getline( file_, line );
                const auto line_tokens = geode::string_split( line );
                absl::Span< const std::string_view > vertex_ids(
                    &line_tokens[1], line_tokens.size() - 1 );
                constexpr geode::index_t physical_entity{ 0 };
                const auto element =
                    geode::internal::GMSHElementFactory::create(
                        mesh_element_type_id, physical_entity, entity_id,
                        vertex_ids );
                element->add_element( brep_, gmsh_id2uuids_ );
            }
        }

        void build_corners()
        {
            for( const auto& c : brep_.corners() )
            {
                builder_.corner_mesh_builder( c.id() )->set_point(
                    0, nodes_[brep_.unique_vertex( { c.component_id(), 0 } )] );
            }
        }

        void build_lines()
        {
            for( const auto& l : brep_.lines() )
            {
                filter_duplicated_line_vertices( l, brep_ );
                auto line_builder = builder_.line_mesh_builder( l.id() );
                for( const auto v : geode::Range{ l.mesh().nb_vertices() } )
                {
                    line_builder->set_point(
                        v, nodes_[brep_.unique_vertex(
                               { l.component_id(), v } )] );
                }
            }
        }

        void build_surfaces()
        {
            for( const auto& surface : brep_.surfaces() )
            {
                filter_duplicated_surface_vertices( surface, brep_ );
                auto surface_builder =
                    builder_.surface_mesh_builder( surface.id() );
                const auto& mesh = surface.mesh();
                for( const auto v : geode::Range{ mesh.nb_vertices() } )
                {
                    surface_builder->set_point(
                        v, nodes_[brep_.unique_vertex(
                               { surface.component_id(), v } )] );
                }
                surface_builder->compute_polygon_adjacencies();
                std::vector< geode::PolygonEdge > polygon_edges;
                for( const auto& line : brep_.internal_lines( surface ) )
                {
                    const auto& edges = line.mesh();
                    for( const auto edge_id : geode::Range{ edges.nb_edges() } )
                    {
                        const auto e0 = edges.edge_vertex( { edge_id, 0 } );
                        const auto e1 = edges.edge_vertex( { edge_id, 1 } );
                        const auto cmvs0 =
                            brep_.component_mesh_vertices( brep_.unique_vertex(
                                { line.component_id(), e0 } ) );
                        const auto cmvs1 =
                            brep_.component_mesh_vertices( brep_.unique_vertex(
                                { line.component_id(), e1 } ) );
                        for( const auto& cmv0 : cmvs0 )
                        {
                            if( cmv0.component_id.id() != surface.id() )
                            {
                                continue;
                            }
                            for( const auto& cmv1 : cmvs1 )
                            {
                                if( cmv1.component_id.id() != surface.id() )
                                {
                                    continue;
                                }
                                if( const auto edge0 =
                                        mesh.polygon_edge_from_vertices(
                                            cmv0.vertex, cmv1.vertex ) )
                                {
                                    polygon_edges.emplace_back( edge0.value() );
                                }
                                if( const auto edge1 =
                                        mesh.polygon_edge_from_vertices(
                                            cmv1.vertex, cmv0.vertex ) )
                                {
                                    polygon_edges.emplace_back( edge1.value() );
                                }
                            }
                        }
                    }
                }
                for( const auto& edge : polygon_edges )
                {
                    surface_builder->unset_polygon_adjacent( edge );
                }
            }
        }

        void build_blocks()
        {
            for( const auto& b : brep_.blocks() )
            {
                filter_duplicated_block_vertices( b, brep_ );
                auto block_builder = builder_.block_mesh_builder( b.id() );
                for( const auto v : geode::Range{ b.mesh().nb_vertices() } )
                {
                    block_builder->set_point(
                        v, nodes_[brep_.unique_vertex(
                               { b.component_id(), v } )] );
                }
                block_builder->compute_polyhedron_adjacencies();
            }
        }

        void update_component_vertex( const geode::Line3D& line,
            geode::EdgedCurveBuilder3D& mesh_builder,
            geode::index_t old_line_vertex_id,
            geode::index_t new_line_vertex_id )
        {
            const auto edges =
                line.mesh().edges_around_vertex( old_line_vertex_id );
            OPENGEODE_ASSERT( edges.size() == 1,
                "By construction, there should be one and only one "
                "edge pointing to each vertex at this point." );
            mesh_builder.set_edge_vertex( edges[0], new_line_vertex_id );
        }

        void update_component_vertex( const geode::Surface3D& surface,
            geode::SurfaceMeshBuilder3D& mesh_builder,
            geode::index_t old_surface_vertex_id,
            geode::index_t new_surface_vertex_id )
        {
            const auto polygons =
                surface.mesh().polygons_around_vertex( old_surface_vertex_id );
            OPENGEODE_ASSERT( polygons.size() == 1,
                "By construction, there should be one and only one "
                "polygon pointing to each vertex at this point." );
            mesh_builder.set_polygon_vertex(
                polygons[0], new_surface_vertex_id );
        }

        void update_component_vertex( const geode::Block3D& block,
            geode::SolidMeshBuilder3D& mesh_builder,
            geode::index_t old_block_vertex_id,
            geode::index_t new_block_vertex_id )
        {
            const auto polyhedra =
                block.mesh().polyhedra_around_vertex( old_block_vertex_id );
            OPENGEODE_ASSERT( polyhedra.size() == 1,
                "By construction, there should be one and only one "
                "polyhedron pointing to each vertex at this point." );
            mesh_builder.set_polyhedron_vertex(
                polyhedra[0], new_block_vertex_id );
        }

        template < typename Component, typename ComponentMeshBuilder >
        void filter_duplicated_vertices( const Component& component,
            const geode::BRep& brep,
            ComponentMeshBuilder& mesh_builder )
        {
            absl::flat_hash_map< geode::index_t, std::vector< geode::index_t > >
                unique2component;
            for( const auto v : geode::Range{ component.mesh().nb_vertices() } )
            {
                unique2component[brep.unique_vertex(
                                     { component.component_id(), v } )]
                    .emplace_back( v );
            }
            for( const auto& uv : unique2component )
            {
                for( const auto i : geode::Range{ 1, uv.second.size() } )
                {
                    update_component_vertex(
                        component, mesh_builder, uv.second[i], uv.second[0] );
                }
            }

            const auto old2new = mesh_builder.delete_isolated_vertices();
            builder_.update_unique_vertices(
                component.component_id(), old2new );
        }

        void filter_duplicated_line_vertices(
            const geode::Line3D& line, geode::BRep& brep )
        {
            auto builder =
                geode::BRepBuilder{ brep }.line_mesh_builder( line.id() );
            filter_duplicated_vertices( line, brep, *builder );
        }

        void filter_duplicated_surface_vertices(
            const geode::Surface3D& surface, geode::BRep& brep )
        {
            auto builder =
                geode::BRepBuilder{ brep }.surface_mesh_builder( surface.id() );
            filter_duplicated_vertices( surface, brep, *builder );
        }

        void filter_duplicated_block_vertices(
            const geode::Block3D& block, geode::BRep& brep )
        {
            auto builder =
                geode::BRepBuilder{ brep }.block_mesh_builder( block.id() );
            filter_duplicated_vertices( block, brep, *builder );
        }

        void add_potential_relationships(
            const std::vector< geode::ComponentMeshVertex >&
                boundary_type_vertices,
            const std::vector< geode::ComponentMeshVertex >&
                incidence_type_vertices,
            boundary_incidences_relations& b2i_relations )
        {
            for( const auto& boundary_vertex : boundary_type_vertices )
            {
                for( const auto& incidence_vertex : incidence_type_vertices )
                {
                    b2i_relations[boundary_vertex.component_id.id()].emplace(
                        incidence_vertex.component_id.id() );
                }
            }
        }

        void filter_potential_relationships(
            const std::vector< geode::ComponentMeshVertex >&
                boundary_type_vertices,
            const std::vector< geode::ComponentMeshVertex >&
                incidence_type_vertices,
            boundary_incidences_relations& b2i_relations )
        {
            for( const auto& boundary_vertex : boundary_type_vertices )
            {
                if( b2i_relations.find( boundary_vertex.component_id.id() )
                    == b2i_relations.end() )
                {
                    continue;
                }
                auto& incidences_in_relations =
                    b2i_relations.at( boundary_vertex.component_id.id() );

                auto it = incidences_in_relations.cbegin();
                while( it != incidences_in_relations.cend() )
                {
                    const auto cur_it = it++;
                    const auto& incidence_id = *cur_it;
                    if( std::find_if( incidence_type_vertices.begin(),
                            incidence_type_vertices.end(),
                            [&incidence_id](
                                const geode::ComponentMeshVertex& cmv ) {
                                return cmv.component_id.id() == incidence_id;
                            } )
                        == incidence_type_vertices.end() )
                    {
                        incidences_in_relations.erase( cur_it );
                    }
                }
            }
        }

        std::tuple< std::vector< geode::ComponentMeshVertex >,
            std::vector< geode::ComponentMeshVertex >,
            std::vector< geode::ComponentMeshVertex >,
            std::vector< geode::ComponentMeshVertex > >
            get_component_mesh_vertices( geode::index_t uv )
        {
            std::vector< geode::ComponentMeshVertex > corners_vertices;
            std::vector< geode::ComponentMeshVertex > lines_vertices;
            std::vector< geode::ComponentMeshVertex > surfaces_vertices;
            std::vector< geode::ComponentMeshVertex > blocks_vertices;
            const auto cmv = brep_.component_mesh_vertices( uv );
            for( const auto v : cmv )
            {
                if( v.component_id.type()
                    == geode::Corner3D::component_type_static() )
                {
                    corners_vertices.push_back( v );
                    continue;
                }
                if( v.component_id.type()
                    == geode::Line3D::component_type_static() )
                {
                    lines_vertices.push_back( v );
                    continue;
                }
                if( v.component_id.type()
                    == geode::Surface3D::component_type_static() )
                {
                    surfaces_vertices.push_back( v );
                    continue;
                }
                if( v.component_id.type()
                    == geode::Block3D::component_type_static() )
                {
                    blocks_vertices.push_back( v );
                    continue;
                }
            }
            return { corners_vertices, lines_vertices, surfaces_vertices,
                blocks_vertices };
        }

    private:
        std::ifstream file_;
        geode::BRep& brep_;
        geode::BRepBuilder builder_;
        bool binary_{ true };
        double version_{ 2 };
        std::vector< std::string > sections_;
        std::vector< geode::Point3D > nodes_;
        geode::internal::GmshId2Uuids gmsh_id2uuids_;
    };
} // namespace

namespace geode
{
    namespace internal
    {
        BRep MSHInput::read()
        {
            BRep brep;
            MSHInputImpl impl( filename(), brep );
            impl.read_file();
            impl.build_geometry();
            impl.build_topology();
            return brep;
        }
    } // namespace internal
} // namespace geode
