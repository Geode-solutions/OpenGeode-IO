/*
 * Copyright (c) 2019 - 2020 Geode-solutions
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

#include <geode/io/model/private/msh_input.h>

#include <fstream>
#include <mutex>
#include <numeric>

#include <absl/container/flat_hash_set.h>

#include <geode/basic/algorithm.h>
#include <geode/basic/common.h>
#include <geode/basic/factory.h>
#include <geode/basic/logger.h>
#include <geode/basic/uuid.h>

#include <geode/geometry/point.h>

#include <geode/mesh/builder/edged_curve_builder.h>
#include <geode/mesh/builder/point_set_builder.h>
#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/builder/polyhedral_solid_builder.h>
#include <geode/mesh/core/edged_curve.h>
#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/polygonal_surface.h>
#include <geode/mesh/core/polyhedral_solid.h>

#include <geode/model/mixin/core/block.h>
#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/mixin/core/surface.h>
#include <geode/model/representation/core/brep.h>

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
            return absl::Hash< std::string >()( gmsh_id.type.get() )
                   ^ absl::Hash< geode::index_t >()( gmsh_id.id );
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
        absl::flat_hash_map< GmshElementID, geode::uuid > elementary_ids;
        absl::flat_hash_map< GmshElementID, geode::uuid > physical_ids;
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
                "[GMSHElement] GMSH tag for elementary entity "
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
            for( const auto n : geode::Range{ nb_vertices() } )
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
            const GmshElementID cur_gmsh_id{
                geode::Corner3D::component_type_static(), elementary_entity_id()
            };
            const auto existing_id =
                id_map.contains_elementary_id( cur_gmsh_id );
            geode::uuid corner_uuid;
            geode::BRepBuilder builder{ brep };
            if( existing_id )
            {
                corner_uuid = id_map.elementary_ids.at( cur_gmsh_id );
            }
            else
            {
                corner_uuid = builder.add_corner();
                id_map.elementary_ids.insert( { cur_gmsh_id, corner_uuid } );
            }

            const auto v_id =
                builder.corner_mesh_builder( corner_uuid )->create_vertex();
            builder.set_unique_vertex(
                { brep.corner( corner_uuid ).component_id(), v_id },
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
            const GmshElementID cur_gmsh_id(
                geode::Line3D::component_type_static(),
                elementary_entity_id() );
            const auto existing_id =
                id_map.contains_elementary_id( cur_gmsh_id );
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
            const auto first_v_id =
                builder.line_mesh_builder( line_uuid )
                    ->create_vertices( vertex_ids().size() );
            const auto edge_id =
                builder.line_mesh_builder( line_uuid )
                    ->create_edge( first_v_id, first_v_id + 1 );

            const auto& line = brep.line( line_uuid );
            for( const auto v_id : geode::Range{ vertex_ids().size() } )
            {
                builder.set_unique_vertex(
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
            const GmshElementID cur_gmsh_id(
                geode::Surface3D::component_type_static(),
                elementary_entity_id() );
            const auto existing_id =
                id_map.contains_elementary_id( cur_gmsh_id );
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
            const auto first_v_id =
                builder.surface_mesh_builder( surface_uuid )
                    ->create_vertices( vertex_ids().size() );
            std::vector< geode::index_t > v_ids( vertex_ids().size() );
            std::iota( v_ids.begin(), v_ids.end(), first_v_id );
            const auto polygon_id =
                builder.surface_mesh_builder( surface_uuid )
                    ->create_polygon( v_ids );

            const auto& surface = brep.surface( surface_uuid );
            for( const auto v_id : geode::Range{ vertex_ids().size() } )
            {
                builder.set_unique_vertex(
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
            const GmshElementID cur_gmsh_id(
                geode::Block3D::component_type_static(),
                elementary_entity_id() );
            const auto existing_id =
                id_map.contains_elementary_id( cur_gmsh_id );
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

            const auto first_v_id =
                builder.block_mesh_builder( block_uuid )
                    ->create_vertices( vertex_ids().size() );
            std::vector< geode::index_t > v_ids( vertex_ids().size() );
            std::iota( v_ids.begin(), v_ids.end(), first_v_id );
            const auto polyhedron_id =
                create_gmsh_polyhedron( builder, block_uuid, v_ids );

            const auto& block = brep.block( block_uuid );
            for( const auto v_id : geode::Range{ vertex_ids().size() } )
            {
                builder.set_unique_vertex(
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
            static const std::vector< std::vector< geode::index_t > >
                gmsh_tetrahedron_faces{ { 0, 1, 2 }, { 0, 2, 3 }, { 1, 3, 2 },
                    { 0, 3, 1 } };
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
            static const std::vector< std::vector< geode::index_t > >
                gmsh_hexahedron_faces{ { 0, 1, 2, 3 }, { 7, 6, 5, 4 },
                    { 0, 3, 7, 4 }, { 1, 5, 6, 2 }, { 2, 6, 7, 3 },
                    { 0, 4, 5, 1 } };
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
            static const std::vector< std::vector< geode::index_t > >
                gmsh_prism_faces{ { 0, 1, 2 }, { 5, 4, 3 }, { 0, 2, 5, 3 },
                    { 0, 3, 4, 1 }, { 1, 4, 5, 2 } };
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
            static const std::vector< std::vector< geode::index_t > >
                gmsh_pyramid_faces{ { 0, 3, 4 }, { 0, 4, 1 }, { 4, 3, 2 },
                    { 1, 4, 2 }, { 0, 1, 2, 3 } };
            return builder.block_mesh_builder( block_uuid )
                ->create_polyhedron( v_ids, gmsh_pyramid_faces );
        }
    };

    void initialize_gmsh_factory()
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
        MSHInputImpl( absl::string_view filename, geode::BRep& brep )
            : file_( filename.data() ), brep_( brep ), builder_{ brep }
        {
            OPENGEODE_EXCEPTION( file_.good(),
                "[MSHInput] Error while opening file: ", filename );
            std::call_once( once_flag, initialize_gmsh_factory );
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
                const auto corners_vertices = brep_.mesh_component_vertices(
                    uv, geode::Corner3D::component_type_static() );
                const auto lines_vertices = brep_.mesh_component_vertices(
                    uv, geode::Line3D::component_type_static() );
                const auto surfaces_vertices = brep_.mesh_component_vertices(
                    uv, geode::Surface3D::component_type_static() );
                const auto blocks_vertices = brep_.mesh_component_vertices(
                    uv, geode::Block3D::component_type_static() );

                add_potential_relationships(
                    corners_vertices, lines_vertices, corner_line_relations );
                add_potential_relationships(
                    lines_vertices, surfaces_vertices, line_surface_relations );
                add_potential_relationships( surfaces_vertices, blocks_vertices,
                    surface_block_relations );
            }
            for( const auto uv : geode::Range{ brep_.nb_unique_vertices() } )
            {
                const auto corners_vertices = brep_.mesh_component_vertices(
                    uv, geode::Corner3D::component_type_static() );
                const auto lines_vertices = brep_.mesh_component_vertices(
                    uv, geode::Line3D::component_type_static() );
                const auto surfaces_vertices = brep_.mesh_component_vertices(
                    uv, geode::Surface3D::component_type_static() );
                const auto blocks_vertices = brep_.mesh_component_vertices(
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
        }

    private:
        geode::index_t version() const
        {
            return static_cast< geode::index_t >( std::floor( version_ ) );
        }

        void first_read( absl::string_view filename )
        {
            std::ifstream reader{ filename.data() };
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
            OPENGEODE_EXCEPTION( string_starts_with( line, keyword ),
                "[MSHInput::check_keyword] Line should starts with \"", keyword,
                "\"" );
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
            throw geode::OpenGeodeException{
                "[MSHInput::go_to_section] Cannot find the section "
                + section_header
            };
        }

        void set_msh_version( const std::string& line )
        {
            std::istringstream iss{ line };
            iss >> version_;
            OPENGEODE_EXCEPTION( version() == 2 || version() == 4,
                "[MSHInput::set_msh_version] Only MSH file format "
                "versions 2 and 4 are supported for now." );
            geode::index_t binary;
            iss >> binary;
            if( binary != 0 )
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
                if( string_starts_with( line, "$" )
                    && !string_starts_with( line, "$End" ) )
                {
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
            geode::index_t nb_corners, nb_lines, nb_surfaces, nb_blocks;
            std::istringstream iss{ line };
            iss >> nb_corners >> nb_lines >> nb_surfaces >> nb_blocks;
            create_corners( nb_corners );
            create_lines( nb_lines );
            create_surfaces( nb_surfaces );
            create_blocks( nb_blocks );
            check_keyword( "$EndEntities" );
        }

        void create_corners( const geode::index_t nb_corners )
        {
            for( const auto unused : geode::Range{ nb_corners } )
            {
                geode_unused( unused );
                std::string line;
                std::getline( file_, line );
                std::istringstream iss{ line };
                geode::index_t corner_msh_id;
                iss >> corner_msh_id;
                const auto corner_uuid = builder_.add_corner();
                gmsh_id2uuids_.elementary_ids[{
                    geode::Corner3D::component_type_static(), corner_msh_id }] =
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
                std::istringstream iss{ line };
                geode::index_t line_msh_id;
                iss >> line_msh_id;
                const auto line_uuid = builder_.add_line();
                gmsh_id2uuids_.elementary_ids[{
                    geode::Line3D::component_type_static(), line_msh_id }] =
                    line_uuid;
                double xmin, xmax, ymin, ymax, zmin, zmax;
                iss >> xmin >> ymin >> zmin >> xmax >> ymax >> zmax;
                geode::index_t nb_physical_tags;
                iss >> nb_physical_tags;
                for( const auto unused : geode::Range{ nb_physical_tags } )
                {
                    geode_unused( unused );
                    geode::index_t physical_tag;
                    iss >> physical_tag;
                }
                geode::index_t nb_boundaries;
                iss >> nb_boundaries;
                for( const auto unused : geode::Range{ nb_boundaries } )
                {
                    geode_unused( unused );
                    geode::signed_index_t boundary_msh_id;
                    iss >> boundary_msh_id;
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
                std::string line;
                std::getline( file_, line );
                std::istringstream iss{ line };
                geode::index_t surface_msh_id;
                iss >> surface_msh_id;
                const auto surface_uuid = builder_.add_surface();
                gmsh_id2uuids_
                    .elementary_ids[{ geode::Surface3D::component_type_static(),
                        surface_msh_id }] = surface_uuid;
                double xmin, xmax, ymin, ymax, zmin, zmax;
                iss >> xmin >> ymin >> zmin >> xmax >> ymax >> zmax;
                geode::index_t nb_physical_tags;
                iss >> nb_physical_tags;
                for( const auto unused : geode::Range{ nb_physical_tags } )
                {
                    geode_unused( unused );
                    geode::index_t physical_tag;
                    iss >> physical_tag;
                }
                geode::index_t nb_boundaries;
                iss >> nb_boundaries;
                for( const auto unused : geode::Range{ nb_boundaries } )
                {
                    geode_unused( unused );
                    geode::signed_index_t boundary_msh_id;
                    iss >> boundary_msh_id;
                    boundary_msh_id = std::abs( boundary_msh_id );
                    builder_.add_line_surface_boundary_relationship(
                        brep_.line( gmsh_id2uuids_.elementary_ids.at(
                            { geode::Line3D::component_type_static(),
                                static_cast< geode::index_t >(
                                    boundary_msh_id ) } ) ),
                        brep_.surface( surface_uuid ) );
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
                std::istringstream iss{ line };
                geode::index_t block_msh_id;
                iss >> block_msh_id;
                const auto block_uuid = builder_.add_block();
                gmsh_id2uuids_.elementary_ids[{
                    geode::Block3D::component_type_static(), block_msh_id }] =
                    block_uuid;
                double xmin, xmax, ymin, ymax, zmin, zmax;
                iss >> xmin >> ymin >> zmin >> xmax >> ymax >> zmax;
                geode::index_t nb_physical_tags;
                iss >> nb_physical_tags;
                for( const auto unused : geode::Range{ nb_physical_tags } )
                {
                    geode_unused( unused );
                    geode::index_t physical_tag;
                    iss >> physical_tag;
                }
                geode::index_t nb_boundaries;
                iss >> nb_boundaries;
                for( const auto unused : geode::Range{ nb_boundaries } )
                {
                    geode_unused( unused );
                    geode::signed_index_t boundary_msh_id;
                    iss >> boundary_msh_id;
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

        geode::Point3D read_node_coordinates( std::istringstream& iss )
        {
            double x, y, z;
            iss >> x >> y >> z;
            geode::Point3D node{ { x, y, z } };
            return node;
        }

        void read_node_section_v2()
        {
            go_to_section( "$Nodes" );
            std::string line;
            std::getline( file_, line );
            const auto nb_nodes = std::stoi( line );
            nodes_.reserve( nb_nodes );
            for( const auto n_id : geode::Range{ nb_nodes } )
            {
                std::getline( file_, line );
                nodes_.push_back( read_node( n_id + OFFSET_START, line ) );
            }
            check_keyword( "$EndNodes" );
            builder_.create_unique_vertices( nb_nodes );
        }

        geode::Point3D read_node(
            geode::index_t expected_node_id, const std::string& line )
        {
            std::istringstream iss{ line };
            geode::index_t file_node_id;
            iss >> file_node_id;
            OPENGEODE_EXCEPTION( expected_node_id == file_node_id,
                "[MSHInput::read_node] Node indices should be "
                "continuous." );
            return read_node_coordinates( iss );
        }

        void read_node_section_v4()
        {
            go_to_section( "$Nodes" );
            std::string line;
            std::getline( file_, line );
            geode::index_t nb_groups, nb_total_nodes, min_node_id, max_node_id;
            std::istringstream iss{ line };
            iss >> nb_groups >> nb_total_nodes >> min_node_id >> max_node_id;
            OPENGEODE_EXCEPTION(
                min_node_id == 1 && max_node_id == nb_total_nodes,
                "[MSHInput::read_node_section_v4] Non continuous node indexing "
                "is not supported for now" );
            nodes_.reserve( nb_total_nodes );
            for( const auto unused : geode::Range{ nb_groups } )
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
            std::istringstream iss{ line };
            geode::index_t entity_dimension, entity_id, parametric, nb_nodes;
            iss >> entity_dimension >> entity_id >> parametric >> nb_nodes;
            OPENGEODE_EXCEPTION( parametric == 0,
                "[MSHInput::read_node_group] Parametric node coordinates "
                "is not supported for now" );
            for( const auto unused : geode::Range{ nb_nodes } )
            {
                geode_unused( unused );
                std::getline( file_, line );
            }
            for( const auto unused : geode::Range{ nb_nodes } )
            {
                geode_unused( unused );
                std::getline( file_, line );
                std::istringstream stream{ line };
                nodes_.push_back( read_node_coordinates( stream ) );
            }
        }

        void read_element_section_v2()
        {
            go_to_section( "$Elements" );
            std::string line;
            std::getline( file_, line );
            const auto nb_elements = std::stoi( line );
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
                "[MSHInput::read_element] Element indices should be "
                "continuous." );

            // Element type
            geode::index_t mesh_element_type_id;
            iss >> mesh_element_type_id;

            // Tags
            geode::index_t nb_tags;
            iss >> nb_tags;
            OPENGEODE_EXCEPTION( nb_tags >= 2, "[MSHInput::read_element] "
                                               "Number of tags for an element "
                                               "should be at least 2." );
            geode::index_t physical_entity;
            iss >> physical_entity; //  collection
            geode::index_t elementary_entity;
            iss >> elementary_entity; // item
            for( const auto t : geode::Range{ 2, nb_tags } )
            {
                geode_unused( t );
                geode::index_t skipped_tag;
                iss >> skipped_tag;
            }
            // TODO: create relation to the parent

            const auto element = GMSHElementFactory::create(
                mesh_element_type_id, physical_entity, elementary_entity, iss );
            element->add_element( brep_, gmsh_id2uuids_ );
        }

        void read_element_section_v4()
        {
            go_to_section( "$Elements" );
            std::string line;
            std::getline( file_, line );
            geode::index_t nb_groups, nb_total_elements, min_element_id,
                max_element_id;
            std::istringstream iss{ line };
            iss >> nb_groups >> nb_total_elements >> min_element_id
                >> max_element_id;
            OPENGEODE_EXCEPTION(
                min_element_id == 1 && max_element_id == nb_total_elements,
                "[MSHInput::read_element_section_v4] Non continuous element "
                "indexing is not supported for now" );
            for( const auto unused : geode::Range{ nb_groups } )
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
            std::istringstream iss{ line };
            geode::index_t entity_dimension, entity_id, mesh_element_type_id,
                nb_elements;
            iss >> entity_dimension >> entity_id >> mesh_element_type_id
                >> nb_elements;
            for( const auto unused : geode::Range{ nb_elements } )
            {
                geode_unused( unused );
                std::getline( file_, line );
                std::istringstream stream{ line };
                geode::index_t element_id;
                stream >> element_id;
                constexpr geode::index_t physical_entity{ 0 };
                const auto element = GMSHElementFactory::create(
                    mesh_element_type_id, physical_entity, entity_id, stream );
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
                for( const auto v : geode::Range{ l.mesh().nb_vertices() } )
                {
                    builder_.line_mesh_builder( l.id() )->set_point(
                        v, nodes_[brep_.unique_vertex(
                               { l.component_id(), v } )] );
                }
            }
        }

        void build_surfaces()
        {
            for( const auto& s : brep_.surfaces() )
            {
                filter_duplicated_surface_vertices( s, brep_ );
                for( const auto v : geode::Range{ s.mesh().nb_vertices() } )
                {
                    builder_.surface_mesh_builder( s.id() )->set_point(
                        v, nodes_[brep_.unique_vertex(
                               { s.component_id(), v } )] );
                }
            }
        }

        void build_blocks()
        {
            for( const auto& b : brep_.blocks() )
            {
                filter_duplicated_block_vertices( b, brep_ );
                for( const auto v : geode::Range{ b.mesh().nb_vertices() } )
                {
                    builder_.block_mesh_builder( b.id() )->set_point(
                        v, nodes_[brep_.unique_vertex(
                               { b.component_id(), v } )] );
                }
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
            geode::PolygonalSurfaceBuilder3D& mesh_builder,
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
            geode::PolyhedralSolidBuilder3D& mesh_builder,
            geode::index_t old_block_vertex_id,
            geode::index_t new_block_vertex_id )
        {
            const auto polyhedra =
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
            for( const auto v : geode::Range{ component.mesh().nb_vertices() } )
            {
                unique2component[brep.unique_vertex(
                                     { component.component_id(), v } )]
                    .emplace_back( v );
            }
            std::vector< bool > delete_duplicated(
                component.mesh().nb_vertices(), false );
            for( const auto uv : unique2component )
            {
                for( const auto i : geode::Range{ 1, uv.second.size() } )
                {
                    update_component_vertex(
                        component, *mesh_builder, uv.second[i], uv.second[0] );
                    delete_duplicated[uv.second[i]] = true;
                }
            }

            std::vector< geode::index_t > updated_unique2component;
            updated_unique2component.reserve( std::count(
                delete_duplicated.begin(), delete_duplicated.end(), false ) );
            for( const auto i : geode::Range{ component.mesh().nb_vertices() } )
            {
                if( delete_duplicated[i] )
                {
                    continue;
                }
                auto ui =
                    brep_.unique_vertex( { component.component_id(), i } );
                updated_unique2component.push_back( ui );
            }

            mesh_builder->delete_vertices( delete_duplicated );

            builder_.unregister_mesh_component( component );
            builder_.register_mesh_component( component );
            for( const auto i : geode::Range{ component.mesh().nb_vertices() } )
            {
                builder_.set_unique_vertex( { component.component_id(), i },
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
            const std::vector< geode::MeshComponentVertex >&
                boundary_type_vertices,
            const std::vector< geode::MeshComponentVertex >&
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
                                const geode::MeshComponentVertex& mcv ) {
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
        double version_{ 2 };
        std::vector< std::string > sections_;
        std::vector< geode::Point3D > nodes_;
        GmshId2Uuids gmsh_id2uuids_;
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void MSHInput::read()
        {
            MSHInputImpl impl( filename(), brep() );
            impl.read_file();
            impl.build_geometry();
            impl.build_topology();
        }
    } // namespace detail
} // namespace geode