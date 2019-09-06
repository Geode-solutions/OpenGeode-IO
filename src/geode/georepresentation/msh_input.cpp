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

#include <geode/model/detail/msh_input.h>

#include <fstream>
#include <mutex>
#include <numeric>
#include <unordered_map>

#include <geode/basic/algorithm.h>
#include <geode/basic/common.h>
#include <geode/basic/factory.h>
#include <geode/basic/logger.h>
#include <geode/basic/point.h>
#include <geode/basic/uuid.h>
#include <geode/model/mixin/core/block.h>
#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/mixin/core/surface.h>
#include <geode/model/representation/core/brep.h>

#include <geode/mesh/builder/edged_curve_builder.h>
#include <geode/mesh/builder/point_set_builder.h>
#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/builder/polyhedral_solid_builder.h>
#include <geode/mesh/core/edged_curve.h>
#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/polygonal_surface.h>
#include <geode/mesh/core/polyhedral_solid.h>

namespace
{
    bool string_starts_with(
        const std::string& string, const std::string& check )
    {
        return !string.compare( 0, check.length(), check );
    }

    struct GmshElementID
    {
        GmshElementID( geode::ComponentType gmsh_type, geode::index_t gmsh_id )
            : type( std::move( gmsh_type ) ), id( gmsh_id )
        {
        }

        bool operator==( const GmshElementID& other ) const
        {
            return type == other.type && id == other.id;
        }
        geode::ComponentType type;
        geode::index_t id;
    };
} // namespace

namespace std
{
    template <>
    struct hash< GmshElementID >
    {
        std::size_t operator()( const GmshElementID& gmsh_id ) const
        {
            return std::hash< std::string >()( gmsh_id.type.get() )
                   ^ std::hash< geode::index_t >()( gmsh_id.id );
        }
    };
} // namespace std

namespace
{
    static constexpr geode::index_t OFFSET_START{ 1 };
    static std::once_flag once_flag;

    struct GmshId2Uuids
    {
        GmshId2Uuids() = default;

        bool contains_elementary_id( const GmshElementID& elementary_id ) const
        {
            return elementary_ids.find( elementary_id ) != elementary_ids.end();
        }

        bool contains_physical_id( const GmshElementID& physical_id ) const
        {
            return physical_ids.find( physical_id ) != physical_ids.end();
        }
        std::unordered_map< GmshElementID, geode::uuid > elementary_ids;
        std::unordered_map< GmshElementID, geode::uuid > physical_ids;
    };

    class GMSHElement
    {
    public:
        GMSHElement( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            geode::index_t nb_vertices,
            std::istringstream& iss_vertices_ids )
            : physical_entity_id_( std::move( physical_entity_id ) ),
              elementary_entity_id_( std::move( elementary_entity_id ) ),
              nb_vertices_( nb_vertices ),
              iss_vertices_ids_( iss_vertices_ids )
        {
            OPENGEODE_EXCEPTION( elementary_entity_id > 0,
                "GMSH tag for elementary entity "
                "(second tag) should not be null" );
            try
            {
                read_vertex_ids();
            }
            catch( ... )
            {
                geode::Logger::error( "Wrong GMSH element number of vertices" );
            }
        }

        virtual ~GMSHElement() = default;

        virtual void add_element( geode::BRep& brep, GmshId2Uuids& id_map ) = 0;

        geode::index_t physical_entity_id()
        {
            return physical_entity_id_;
        }

        geode::index_t elementary_entity_id()
        {
            return elementary_entity_id_;
        }

    protected:
        geode::index_t& nb_vertices()
        {
            return nb_vertices_;
        }

        std::vector< geode::index_t >& vertex_ids()
        {
            return vertex_ids_;
        }

        void read_vertex_ids()
        {
            geode::index_t vertex_id;
            vertex_ids().resize( nb_vertices() );
            for( auto n : geode::Range{ nb_vertices() } )
            {
                iss_vertices_ids_ >> vertex_id;
                vertex_ids()[n] = vertex_id;
            }
        }

    private:
        geode::index_t physical_entity_id_;
        geode::index_t elementary_entity_id_;
        geode::index_t nb_vertices_;
        std::istringstream& iss_vertices_ids_;
        std::vector< geode::index_t > vertex_ids_;
    };

    using GMSHElementFactory = geode::Factory< geode::index_t,
        GMSHElement,
        geode::index_t,
        geode::index_t,
        std::istringstream& >;

    class GMSHPoint : public GMSHElement
    {
    public:
        GMSHPoint( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            std::istringstream& iss_vertices_ids )
            : GMSHElement{ physical_entity_id, elementary_entity_id, 1,
                  iss_vertices_ids }
        {
        }
        void add_element( geode::BRep& brep, GmshId2Uuids& id_map ) final
        {
            GmshElementID cur_gmsh_id{ geode::Corner3D::component_type_static(),
                elementary_entity_id() };
            auto existing_id = id_map.contains_elementary_id( cur_gmsh_id );
            OPENGEODE_EXCEPTION( !existing_id,
                "At least two points (type=15) has the same tag "
                "for elementary entity. This is not supported." );
            geode::BRepBuilder builder{ brep };
            auto new_corner_uuid = builder.add_corner();
            auto v_id =
                builder.corner_mesh_builder( new_corner_uuid )->create_vertex();
            id_map.elementary_ids.insert( { cur_gmsh_id, new_corner_uuid } );
            builder.unique_vertices().set_unique_vertex(
                { brep.corner( new_corner_uuid ).component_id(), v_id },
                vertex_ids()[0] - OFFSET_START );
        }
    };

    class GMSHEdge : public GMSHElement
    {
    public:
        GMSHEdge( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            std::istringstream& iss_vertices_id )
            : GMSHElement{ physical_entity_id, elementary_entity_id, 2,
                  iss_vertices_id }
        {
        }

        void add_element( geode::BRep& brep, GmshId2Uuids& id_map ) final
        {
            GmshElementID cur_gmsh_id( geode::Line3D::component_type_static(),
                elementary_entity_id() );
            auto existing_id = id_map.contains_elementary_id( cur_gmsh_id );
            geode::BRepBuilder builder{ brep };
            geode::uuid line_uuid;
            if( existing_id )
            {
                line_uuid = id_map.elementary_ids.at( cur_gmsh_id );
            }
            else
            {
                line_uuid = builder.add_line();
                id_map.elementary_ids.insert( { cur_gmsh_id, line_uuid } );
            }
            auto first_v_id = builder.line_mesh_builder( line_uuid )
                                  ->create_vertices( vertex_ids().size() );
            auto edge_id = builder.line_mesh_builder( line_uuid )
                               ->create_edge( first_v_id, first_v_id + 1 );

            const auto& line = brep.line( line_uuid );
            for( auto v_id : geode::Range{ vertex_ids().size() } )
            {
                builder.unique_vertices().set_unique_vertex(
                    { line.component_id(),
                        line.mesh().edge_vertex( { edge_id, v_id } ) },
                    vertex_ids()[v_id] - OFFSET_START );
            }
        }
    };

    class GMSHSurfacePolygon : public GMSHElement
    {
    public:
        GMSHSurfacePolygon( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            geode::index_t nb_vertices,
            std::istringstream& iss_vertices_id )
            : GMSHElement{ physical_entity_id, elementary_entity_id,
                  nb_vertices, iss_vertices_id }
        {
        }

        void add_element( geode::BRep& brep, GmshId2Uuids& id_map ) final
        {
            GmshElementID cur_gmsh_id(
                geode::Surface3D::component_type_static(),
                elementary_entity_id() );
            auto existing_id = id_map.contains_elementary_id( cur_gmsh_id );
            geode::BRepBuilder builder{ brep };
            geode::uuid surface_uuid;
            if( existing_id )
            {
                surface_uuid = id_map.elementary_ids.at( cur_gmsh_id );
            }
            else
            {
                surface_uuid = builder.add_surface();
                id_map.elementary_ids.insert( { cur_gmsh_id, surface_uuid } );
            }
            auto first_v_id = builder.surface_mesh_builder( surface_uuid )
                                  ->create_vertices( vertex_ids().size() );
            std::vector< geode::index_t > v_ids( vertex_ids().size() );
            std::iota( v_ids.begin(), v_ids.end(), first_v_id );
            auto polygon_id = builder.surface_mesh_builder( surface_uuid )
                                  ->create_polygon( v_ids );

            const auto& surface = brep.surface( surface_uuid );
            for( auto v_id : geode::Range{ vertex_ids().size() } )
            {
                builder.unique_vertices().set_unique_vertex(
                    { surface.component_id(),
                        surface.mesh().polygon_vertex( { polygon_id, v_id } ) },
                    vertex_ids()[v_id] - OFFSET_START );
            }
        }
    };

    class GMSHTriangle : public GMSHSurfacePolygon
    {
    public:
        GMSHTriangle( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            std::istringstream& iss_vertices_id )
            : GMSHSurfacePolygon{ physical_entity_id, elementary_entity_id, 3,
                  iss_vertices_id }
        {
        }
    };

    class GMSHQuadrangle : public GMSHSurfacePolygon
    {
    public:
        GMSHQuadrangle( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            std::istringstream& iss_vertices_id )
            : GMSHSurfacePolygon{ physical_entity_id, elementary_entity_id, 4,
                  iss_vertices_id }
        {
        }
    };

    class GMSHSolidPolyhedron : public GMSHElement
    {
    public:
        GMSHSolidPolyhedron( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            geode::index_t nb_vertices,
            std::istringstream& iss_vertices_id )
            : GMSHElement{ physical_entity_id, elementary_entity_id,
                  nb_vertices, iss_vertices_id }
        {
        }

        virtual geode::index_t create_gmsh_polyhedron(
            geode::BRepBuilder& builder,
            const geode::uuid& block_uuid,
            const std::vector< geode::index_t >& v_ids ) = 0;

        void add_element( geode::BRep& brep, GmshId2Uuids& id_map ) final
        {
            GmshElementID cur_gmsh_id( geode::Block3D::component_type_static(),
                elementary_entity_id() );
            auto existing_id = id_map.contains_elementary_id( cur_gmsh_id );
            geode::BRepBuilder builder{ brep };
            geode::uuid block_uuid;
            if( existing_id )
            {
                block_uuid = id_map.elementary_ids.at( cur_gmsh_id );
            }
            else
            {
                block_uuid = builder.add_block();
                id_map.elementary_ids.insert( { cur_gmsh_id, block_uuid } );
            }

            auto first_v_id = builder.block_mesh_builder( block_uuid )
                                  ->create_vertices( vertex_ids().size() );
            std::vector< geode::index_t > v_ids( vertex_ids().size() );
            std::iota( v_ids.begin(), v_ids.end(), first_v_id );
            auto polyhedron_id =
                create_gmsh_polyhedron( builder, block_uuid, v_ids );

            const auto& block = brep.block( block_uuid );
            for( auto v_id : geode::Range{ vertex_ids().size() } )
            {
                builder.unique_vertices().set_unique_vertex(
                    { block.component_id(), block.mesh().polyhedron_vertex(
                                                { polyhedron_id, v_id } ) },
                    vertex_ids()[v_id] - OFFSET_START );
            }
        }
    };

    class GMSHTetrahedron : public GMSHSolidPolyhedron
    {
    public:
        GMSHTetrahedron( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            std::istringstream& iss_vertices_id )
            : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id, 4,
                  iss_vertices_id }
        {
        }

        geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
            const geode::uuid& block_uuid,
            const std::vector< geode::index_t >& v_ids ) override final
        {
            std::vector< std::vector< geode::index_t > > gmsh_tetrahedron_faces(
                4 );
            gmsh_tetrahedron_faces[0] = { v_ids[0], v_ids[1], v_ids[2] };
            gmsh_tetrahedron_faces[1] = { v_ids[0], v_ids[2], v_ids[3] };
            gmsh_tetrahedron_faces[2] = { v_ids[1], v_ids[3], v_ids[2] };
            gmsh_tetrahedron_faces[3] = { v_ids[0], v_ids[3], v_ids[1] };
            return builder.block_mesh_builder( block_uuid )
                ->create_polyhedron( v_ids, gmsh_tetrahedron_faces );
        }
    };

    class GMSHHexahedron : public GMSHSolidPolyhedron
    {
    public:
        GMSHHexahedron( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            std::istringstream& iss_vertices_id )
            : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id, 8,
                  iss_vertices_id }
        {
        }

        geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
            const geode::uuid& block_uuid,
            const std::vector< geode::index_t >& v_ids ) override final
        {
            std::vector< std::vector< geode::index_t > > gmsh_hexahedron_faces(
                6 );
            gmsh_hexahedron_faces[0] = { v_ids[0], v_ids[1], v_ids[2],
                v_ids[3] };
            gmsh_hexahedron_faces[1] = { v_ids[7], v_ids[6], v_ids[5],
                v_ids[4] };
            gmsh_hexahedron_faces[2] = { v_ids[0], v_ids[3], v_ids[7],
                v_ids[4] };
            gmsh_hexahedron_faces[3] = { v_ids[1], v_ids[5], v_ids[6],
                v_ids[2] };
            gmsh_hexahedron_faces[4] = { v_ids[2], v_ids[6], v_ids[7],
                v_ids[3] };
            gmsh_hexahedron_faces[5] = { v_ids[0], v_ids[4], v_ids[5],
                v_ids[1] };
            return builder.block_mesh_builder( block_uuid )
                ->create_polyhedron( v_ids, gmsh_hexahedron_faces );
        }
    };

    class GMSHPrism : public GMSHSolidPolyhedron
    {
    public:
        GMSHPrism( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            std::istringstream& iss_vertices_id )
            : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id, 6,
                  iss_vertices_id }
        {
        }

        geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
            const geode::uuid& block_uuid,
            const std::vector< geode::index_t >& v_ids ) override final
        {
            std::vector< std::vector< geode::index_t > > gmsh_prism_faces( 5 );
            gmsh_prism_faces[0] = { v_ids[0], v_ids[1], v_ids[2] };
            gmsh_prism_faces[1] = { v_ids[5], v_ids[4], v_ids[3] };
            gmsh_prism_faces[2] = { v_ids[0], v_ids[2], v_ids[5], v_ids[3] };
            gmsh_prism_faces[3] = { v_ids[0], v_ids[3], v_ids[4], v_ids[1] };
            gmsh_prism_faces[4] = { v_ids[1], v_ids[4], v_ids[5], v_ids[2] };
            return builder.block_mesh_builder( block_uuid )
                ->create_polyhedron( v_ids, gmsh_prism_faces );
        }
    };

    class GMSHPyramid : public GMSHSolidPolyhedron
    {
    public:
        GMSHPyramid( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            std::istringstream& iss_vertices_id )
            : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id, 5,
                  iss_vertices_id }
        {
        }

        geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
            const geode::uuid& block_uuid,
            const std::vector< geode::index_t >& v_ids ) override final
        {
            std::vector< std::vector< geode::index_t > > gmsh_pyramid_faces(
                5 );
            gmsh_pyramid_faces[0] = { v_ids[0], v_ids[3], v_ids[4] };
            gmsh_pyramid_faces[1] = { v_ids[0], v_ids[4], v_ids[1] };
            gmsh_pyramid_faces[2] = { v_ids[4], v_ids[3], v_ids[2] };
            gmsh_pyramid_faces[3] = { v_ids[1], v_ids[4], v_ids[2] };
            gmsh_pyramid_faces[4] = { v_ids[0], v_ids[1], v_ids[2], v_ids[3] };
            return builder.block_mesh_builder( block_uuid )
                ->create_polyhedron( v_ids, gmsh_pyramid_faces );
        }
    };

    void initialiaze_gmsh_factory()
    {
        GMSHElementFactory::register_creator< GMSHPoint >( 15 );
        GMSHElementFactory::register_creator< GMSHEdge >( 1 );
        GMSHElementFactory::register_creator< GMSHTriangle >( 2 );
        GMSHElementFactory::register_creator< GMSHQuadrangle >( 3 );
        GMSHElementFactory::register_creator< GMSHTetrahedron >( 4 );
        GMSHElementFactory::register_creator< GMSHHexahedron >( 5 );
        GMSHElementFactory::register_creator< GMSHPrism >( 6 );
        GMSHElementFactory::register_creator< GMSHPyramid >( 7 );
    }

    class MSHInputImpl
    {
    public:
        MSHInputImpl( std::string filename, geode::BRep& brep )
            : file_( std::move( filename ) ), brep_( brep ), builder_{ brep }
        {
            OPENGEODE_EXCEPTION(
                file_.good(), "Error while opening file: " + filename );
        }

        void read_file()
        {
            std::call_once( once_flag, initialiaze_gmsh_factory );
            read_header();
            read_node_section();
            read_element_section();
        }

        void build_geometry()
        {
            build_corners();
            build_lines();
            build_surfaces();
            build_blocks();
        }

        using boundary_incidences_relations = std::unordered_map< geode::uuid,
            std::unordered_set< geode::uuid > >;

        void build_topology()
        {
            const auto& model_vertices = brep_.unique_vertices();

            boundary_incidences_relations corner_line_relations;
            boundary_incidences_relations line_surface_relations;
            boundary_incidences_relations surface_block_relations;

            for( auto uv : geode::Range{ model_vertices.nb_unique_vertices() } )
            {
                const auto corners_vertices =
                    model_vertices.mesh_component_vertices(
                        uv, geode::Corner3D::component_type_static() );
                const auto lines_vertices =
                    model_vertices.mesh_component_vertices(
                        uv, geode::Line3D::component_type_static() );
                const auto surfaces_vertices =
                    model_vertices.mesh_component_vertices(
                        uv, geode::Surface3D::component_type_static() );
                const auto blocks_vertices =
                    model_vertices.mesh_component_vertices(
                        uv, geode::Block3D::component_type_static() );

                add_potential_relationships(
                    corners_vertices, lines_vertices, corner_line_relations );
                add_potential_relationships(
                    lines_vertices, surfaces_vertices, line_surface_relations );
                add_potential_relationships( surfaces_vertices, blocks_vertices,
                    surface_block_relations );
            }
            for( auto uv : geode::Range{ model_vertices.nb_unique_vertices() } )
            {
                const auto corners_vertices =
                    model_vertices.mesh_component_vertices(
                        uv, geode::Corner3D::component_type_static() );
                const auto lines_vertices =
                    model_vertices.mesh_component_vertices(
                        uv, geode::Line3D::component_type_static() );
                const auto surfaces_vertices =
                    model_vertices.mesh_component_vertices(
                        uv, geode::Surface3D::component_type_static() );
                const auto blocks_vertices =
                    model_vertices.mesh_component_vertices(
                        uv, geode::Block3D::component_type_static() );

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
                    builder_.add_boundary_relation(
                        brep_.corner( c2l.first ), brep_.line( line_id ) );
                }
            }
            for( const auto& l2s : line_surface_relations )
            {
                for( const auto& surface_id : l2s.second )
                {
                    builder_.add_boundary_relation(
                        brep_.line( l2s.first ), brep_.surface( surface_id ) );
                }
            }
            for( const auto& s2b : surface_block_relations )
            {
                for( const auto& block_id : s2b.second )
                {
                    builder_.add_boundary_relation(
                        brep_.surface( s2b.first ), brep_.block( block_id ) );
                }
            }
        }

    private:
        void check_keyword( const std::string& keyword )
        {
            std::string line;
            std::getline( file_, line );
            OPENGEODE_EXCEPTION( string_starts_with( line, keyword ),
                "Line should starts with \"" + keyword + "\"" );
        }

        void go_to_section( const std::string& section_header )
        {
            std::string line;
            while( std::getline( file_, line ) )
            {
                if( string_starts_with( line, section_header ) )
                {
                    return;
                }
            }
            throw geode::OpenGeodeException(
                "Cannot find the section " + section_header );
        }

        void check_msh_version( const std::string& line )
        {
            std::istringstream iss{ line };
            double nb_version;
            iss >> nb_version;
            OPENGEODE_EXCEPTION( std::floor( nb_version ) == 2,
                "Only MSH file format version 2 is supported for now." );
            geode::index_t binary;
            iss >> binary;
            if( binary != 0 )
            {
                binary_ = false;
                throw geode::OpenGeodeException(
                    "Binary format is not supported for now." );
            }
        }

        void read_header()
        {
            check_keyword( "$MeshFormat" );
            std::string line;
            std::getline( file_, line );
            check_msh_version( line );
            check_keyword( "$EndMeshFormat" );
        }

        void read_node_section()
        {
            go_to_section( "$Nodes" );
            std::string line;
            std::getline( file_, line );
            auto nb_nodes = std::stoi( line );
            nodes_.reserve( nb_nodes );
            for( auto n_id : geode::Range{ nb_nodes } )
            {
                std::getline( file_, line );
                nodes_.push_back( read_node( n_id + OFFSET_START, line ) );
            }
            check_keyword( "$EndNodes" );
            builder_.unique_vertices().create_unique_vertices( nb_nodes );
        }

        geode::Point3D read_node(
            geode::index_t expected_node_id, const std::string& line )
        {
            std::istringstream iss{ line };
            geode::index_t file_node_id;
            iss >> file_node_id;
            OPENGEODE_EXCEPTION( expected_node_id == file_node_id,
                "Node indices should be continuous." );
            geode::Point3D node;
            for( auto c : geode::Range{ 3 } )
            {
                double coord;
                iss >> coord;
                node.set_value( c, coord );
            }
            return node;
        }

        void read_element_section()
        {
            go_to_section( "$Elements" );
            std::string line;
            std::getline( file_, line );
            auto nb_elements = std::stoi( line );
            for( auto e_id : geode::Range{ nb_elements } )
            {
                std::getline( file_, line );
                read_element( e_id + OFFSET_START, line );
            }
            check_keyword( "$EndElements" );
        }

        void read_element(
            geode::index_t expected_element_id, const std::string& line )
        {
            std::istringstream iss{ line };
            geode::index_t line_element_id;
            iss >> line_element_id;
            OPENGEODE_EXCEPTION( expected_element_id == line_element_id,
                "Element indices should be continuous." );

            // Element type
            geode::index_t mesh_element_type_id;
            iss >> mesh_element_type_id;

            // Tags
            geode::index_t nb_tags;
            iss >> nb_tags;
            OPENGEODE_EXCEPTION( nb_tags >= 2,
                "Number of tags for an element should be at least 2." );
            geode::index_t physical_entity;
            iss >> physical_entity; //  collection
            geode::index_t elementary_entity;
            iss >> elementary_entity; // item
            for( auto t : geode::Range{ 2, nb_tags } )
            {
                geode_unused( t );
                geode::index_t skipped_tag;
                iss >> skipped_tag;
            }
            // TODO: create relation to the parent

            auto element = GMSHElementFactory::create(
                mesh_element_type_id, physical_entity, elementary_entity, iss );
            element->add_element( brep_, gmsh_id2uuids_ );
        }

        void build_corners()
        {
            for( const auto& c : brep_.corners() )
            {
                builder_.corner_mesh_builder( c.id() )->set_point(
                    0, nodes_[brep_.unique_vertices().unique_vertex(
                           { c.component_id(), 0 } )] );
            }
        }

        void build_lines()
        {
            for( const auto& l : brep_.lines() )
            {
                filter_duplicated_line_vertices( l, brep_ );
                for( auto v : geode::Range{ l.mesh().nb_vertices() } )
                {
                    builder_.line_mesh_builder( l.id() )->set_point(
                        v, nodes_[brep_.unique_vertices().unique_vertex(
                               { l.component_id(), v } )] );
                }
            }
        }

        void build_surfaces()
        {
            for( const auto& s : brep_.surfaces() )
            {
                filter_duplicated_surface_vertices( s, brep_ );
                for( auto v : geode::Range{ s.mesh().nb_vertices() } )
                {
                    builder_.surface_mesh_builder( s.id() )->set_point(
                        v, nodes_[brep_.unique_vertices().unique_vertex(
                               { s.component_id(), v } )] );
                }
            }
        }

        void build_blocks()
        {
            for( const auto& b : brep_.blocks() )
            {
                filter_duplicated_block_vertices( b, brep_ );
                for( auto v : geode::Range{ b.mesh().nb_vertices() } )
                {
                    builder_.block_mesh_builder( b.id() )->set_point(
                        v, nodes_[brep_.unique_vertices().unique_vertex(
                               { b.component_id(), v } )] );
                }
            }
        }

        void update_component_vertex( const geode::Line3D& line,
            geode::EdgedCurveBuilder3D& mesh_builder,
            geode::index_t old_line_vertex_id,
            geode::index_t new_line_vertex_id )
        {
            auto edges = line.mesh().edges_around_vertex( old_line_vertex_id );
            OPENGEODE_ASSERT( edges.size() == 1,
                "By construction, there should be one and only one "
                "edge pointing to each vertex at this point." );
            mesh_builder.set_edge_vertex( edges[0], new_line_vertex_id );
        }

        void update_component_vertex( const geode::Surface3D& surface,
            geode::PolygonalSurfaceBuilder3D& mesh_builder,
            geode::index_t old_surface_vertex_id,
            geode::index_t new_surface_vertex_id )
        {
            auto polygons =
                surface.mesh().polygons_around_vertex( old_surface_vertex_id );
            OPENGEODE_ASSERT( polygons.size() == 1,
                "By construction, there should be one and only one "
                "polygon pointing to each vertex at this point." );
            mesh_builder.set_polygon_vertex(
                polygons[0], new_surface_vertex_id );
        }

        void update_component_vertex( const geode::Block3D& block,
            geode::PolyhedralSolidBuilder3D& mesh_builder,
            geode::index_t old_block_vertex_id,
            geode::index_t new_block_vertex_id )
        {
            auto polyhedra =
                block.mesh().polyhedra_around_vertex( old_block_vertex_id );
            OPENGEODE_ASSERT( polyhedra.size() == 1,
                "By construction, there should be one and only one "
                "polyhedra pointing to each vertex at this point." );
            mesh_builder.set_polyhedron_vertex(
                polyhedra[0], new_block_vertex_id );
        }

        template < typename Component, typename ComponentMeshBuilder >
        void filter_duplicated_vertices( const Component& component,
            geode::BRep& brep,
            const ComponentMeshBuilder& mesh_builder )
        {
            std::unordered_map< geode::index_t, std::vector< geode::index_t > >
                unique2component;
            for( auto v : geode::Range{ component.mesh().nb_vertices() } )
            {
                unique2component[brep.unique_vertices().unique_vertex(
                                     { component.component_id(), v } )]
                    .emplace_back( v );
            }
            std::vector< bool > delete_duplicated(
                component.mesh().nb_vertices(), false );
            for( auto uv : unique2component )
            {
                for( auto i : geode::Range{ 1, uv.second.size() } )
                {
                    update_component_vertex(
                        component, *mesh_builder, uv.second[i], uv.second[0] );
                    delete_duplicated[uv.second[i]] = true;
                }
            }

            std::vector< geode::index_t > updated_unique2component;
            updated_unique2component.reserve( std::count(
                delete_duplicated.begin(), delete_duplicated.end(), false ) );
            for( auto i : geode::Range{ component.mesh().nb_vertices() } )
            {
                if( delete_duplicated[i] )
                {
                    continue;
                }
                auto ui = brep_.unique_vertices().unique_vertex(
                    { component.component_id(), i } );
                updated_unique2component.push_back( ui );
            }

            mesh_builder->delete_vertices( delete_duplicated );

            builder_.unique_vertices().unregister_mesh_component( component );
            builder_.unique_vertices().register_mesh_component( component );
            for( auto i : geode::Range{ component.mesh().nb_vertices() } )
            {
                builder_.unique_vertices().set_unique_vertex(
                    { component.component_id(), i },
                    updated_unique2component[i] );
            }
        }

        void filter_duplicated_line_vertices(
            const geode::Line3D& line, geode::BRep& brep )
        {
            filter_duplicated_vertices( line, brep,
                geode::BRepBuilder{ brep }.line_mesh_builder( line.id() ) );
        }

        void filter_duplicated_surface_vertices(
            const geode::Surface3D& surface, geode::BRep& brep )
        {
            filter_duplicated_vertices( surface, brep,
                geode::BRepBuilder{ brep }.surface_mesh_builder(
                    surface.id() ) );
        }

        void filter_duplicated_block_vertices(
            const geode::Block3D& block, geode::BRep& brep )
        {
            filter_duplicated_vertices( block, brep,
                geode::BRepBuilder{ brep }.block_mesh_builder( block.id() ) );
        }

        void add_potential_relationships(
            const std::vector< geode::MeshComponentVertex >&
                boundary_type_vertices,
            const std::vector< geode::MeshComponentVertex >&
                incidence_type_vertices,
            boundary_incidences_relations& b2i_relations )
        {
            for( auto& boundary_vertex : boundary_type_vertices )
            {
                for( auto& incidence_vertex : incidence_type_vertices )
                {
                    b2i_relations[boundary_vertex.component_id.id()].emplace(
                        incidence_vertex.component_id.id() );
                }
            }
        }

        void filter_potential_relationships(
            const std::vector< geode::MeshComponentVertex >&
                boundary_type_vertices,
            const std::vector< geode::MeshComponentVertex >&
                incidence_type_vertices,
            boundary_incidences_relations& b2i_relations )
        {
            for( auto& boundary_vertex : boundary_type_vertices )
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
                    auto cur_it = it++;
                    const auto& incidence_id = *cur_it;
                    if( std::find_if( incidence_type_vertices.begin(),
                            incidence_type_vertices.end(),
                            [&, incidence_id](
                                const geode::MeshComponentVertex& mcv )
                                -> bool {
                                return mcv.component_id.id() == incidence_id;
                            } )
                        == incidence_type_vertices.end() )
                    {
                        incidences_in_relations.erase( cur_it );
                    }
                }
            }
        }

    private:
        std::ifstream file_;
        geode::BRep& brep_;
        geode::BRepBuilder builder_;
        bool binary_{ true };
        std::vector< geode::Point3D > nodes_;
        GmshId2Uuids gmsh_id2uuids_;
    };
} // namespace

namespace geode
{
    void MSHInput::read()
    {
        MSHInputImpl impl( filename(), brep() );
        impl.read_file();
        impl.build_geometry();
        impl.build_topology();
    }
} // namespace geode
