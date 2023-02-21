/*
 * Copyright (c) 2019 - 2023 Geode-solutions
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
#include <absl/strings/str_split.h>

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
#include <geode/mesh/core/hybrid_solid.h>
#include <geode/mesh/core/mesh_factory.h>
#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/polygonal_surface.h>

#include <geode/model/mixin/core/block.h>
#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/mixin/core/surface.h>
#include <geode/model/representation/builder/brep_builder.h>
#include <geode/model/representation/core/brep.h>

namespace
{
    bool string_starts_with(
        const std::string& string, const std::string& check )
    {
        return !string.compare( 0, check.length(), check );
    }

    std::vector< absl::string_view > get_tokens( absl::string_view line )
    {
        return absl::StrSplit( line, ' ' );
    }

    geode::index_t string_to_index( absl::string_view token )
    {
        geode::index_t result;
        const auto ok = absl::SimpleAtoi( token, &result );
        OPENGEODE_EXCEPTION( ok, "[string_to_index] Error while file reading" );
        return result;
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
            absl::Span< const absl::string_view > vertex_ids )
            : physical_entity_id_( std::move( physical_entity_id ) ),
              elementary_entity_id_( std::move( elementary_entity_id ) ),
              nb_vertices_( nb_vertices ),
              vertex_ids_str_( vertex_ids )
        {
            OPENGEODE_EXCEPTION( elementary_entity_id > 0,
                "[GMSHElement] GMSH tag for elementary entity "
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
            vertex_ids().resize( nb_vertices() );
            for( const auto n : geode::Range{ nb_vertices() } )
            {
                vertex_ids()[n] = string_to_index( vertex_ids_str_[n] );
            }
        }

    private:
        geode::index_t physical_entity_id_;
        geode::index_t elementary_entity_id_;
        geode::index_t nb_vertices_;
        absl::Span< const absl::string_view > vertex_ids_str_;
        std::vector< geode::index_t > vertex_ids_;
    };

    using GMSHElementFactory = geode::Factory< geode::index_t,
        GMSHElement,
        geode::index_t,
        geode::index_t,
        absl::Span< const absl::string_view > >;

    class GMSHPoint : public GMSHElement
    {
    public:
        GMSHPoint( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHElement{ physical_entity_id, elementary_entity_id, 1,
                  vertex_ids }
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
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHElement{ physical_entity_id, elementary_entity_id, 2,
                  vertex_ids }
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
            for( const auto v_id : geode::LIndices{ vertex_ids() } )
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
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHElement{ physical_entity_id, elementary_entity_id,
                  nb_vertices, vertex_ids }
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
            for( const auto v_id : geode::LIndices{ vertex_ids() } )
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
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHSurfacePolygon{ physical_entity_id, elementary_entity_id, 3,
                  vertex_ids }
        {
        }
    };

    class GMSHQuadrangle : public GMSHSurfacePolygon
    {
    public:
        GMSHQuadrangle( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHSurfacePolygon{ physical_entity_id, elementary_entity_id, 4,
                  vertex_ids }
        {
        }
    };

    class GMSHSolidPolyhedron : public GMSHElement
    {
    public:
        GMSHSolidPolyhedron( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            geode::index_t nb_vertices,
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHElement{ physical_entity_id, elementary_entity_id,
                  nb_vertices, vertex_ids }
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
                block_uuid =
                    builder.add_block( geode::MeshFactory::default_impl(
                        geode::HybridSolid3D::type_name_static() ) );
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
            for( const auto v_id : geode::LIndices{ vertex_ids() } )
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
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id, 4,
                  vertex_ids }
        {
        }

        geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
            const geode::uuid& block_uuid,
            const std::vector< geode::index_t >& v_ids ) override final
        {
            static const std::array< std::vector< geode::local_index_t >, 4 >
                gmsh_tetrahedron_faces{ { { 0, 1, 2 }, { 0, 2, 3 }, { 1, 3, 2 },
                    { 0, 3, 1 } } };
            return builder.block_mesh_builder( block_uuid )
                ->create_polyhedron( v_ids, gmsh_tetrahedron_faces );
        }
    };

    class GMSHHexahedron : public GMSHSolidPolyhedron
    {
    public:
        GMSHHexahedron( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id, 8,
                  vertex_ids }
        {
        }

        geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
            const geode::uuid& block_uuid,
            const std::vector< geode::index_t >& v_ids ) override final
        {
            static const std::array< std::vector< geode::local_index_t >, 6 >
                gmsh_hexahedron_faces{ { { 0, 1, 2, 3 }, { 7, 6, 5, 4 },
                    { 0, 3, 7, 4 }, { 1, 5, 6, 2 }, { 2, 6, 7, 3 },
                    { 0, 4, 5, 1 } } };
            return builder.block_mesh_builder( block_uuid )
                ->create_polyhedron( v_ids, gmsh_hexahedron_faces );
        }
    };

    class GMSHPrism : public GMSHSolidPolyhedron
    {
    public:
        GMSHPrism( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id, 6,
                  vertex_ids }
        {
        }

        geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
            const geode::uuid& block_uuid,
            const std::vector< geode::index_t >& v_ids ) override final
        {
            static const std::array< std::vector< geode::local_index_t >, 5 >
                gmsh_prism_faces{ { { 0, 1, 2 }, { 5, 4, 3 }, { 0, 2, 5, 3 },
                    { 0, 3, 4, 1 }, { 1, 4, 5, 2 } } };
            return builder.block_mesh_builder( block_uuid )
                ->create_polyhedron( v_ids, gmsh_prism_faces );
        }
    };

    class GMSHPyramid : public GMSHSolidPolyhedron
    {
    public:
        GMSHPyramid( geode::index_t physical_entity_id,
            geode::index_t elementary_entity_id,
            absl::Span< const absl::string_view > vertex_ids )
            : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id, 5,
                  vertex_ids }
        {
        }

        geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
            const geode::uuid& block_uuid,
            const std::vector< geode::index_t >& v_ids ) override final
        {
            static const std::array< std::vector< geode::local_index_t >, 5 >
                gmsh_pyramid_faces{ { { 0, 3, 4 }, { 0, 4, 1 }, { 4, 3, 2 },
                    { 1, 4, 2 }, { 0, 1, 2, 3 } } };
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
            : file_{ geode::to_string( filename ) },
              brep_( brep ),
              builder_{ brep }
        {
            OPENGEODE_EXCEPTION( file_.good(),
                "[MSHInput] Error while opening file: ", filename );
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
                const auto corners_vertices = brep_.component_mesh_vertices(
                    uv, geode::Corner3D::component_type_static() );
                const auto lines_vertices = brep_.component_mesh_vertices(
                    uv, geode::Line3D::component_type_static() );
                const auto surfaces_vertices = brep_.component_mesh_vertices(
                    uv, geode::Surface3D::component_type_static() );
                const auto blocks_vertices = brep_.component_mesh_vertices(
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
                const auto corners_vertices = brep_.component_mesh_vertices(
                    uv, geode::Corner3D::component_type_static() );
                const auto lines_vertices = brep_.component_mesh_vertices(
                    uv, geode::Line3D::component_type_static() );
                const auto surfaces_vertices = brep_.component_mesh_vertices(
                    uv, geode::Surface3D::component_type_static() );
                const auto blocks_vertices = brep_.component_mesh_vertices(
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
            const auto header_tokens = get_tokens( line );
            const auto ok = absl::SimpleAtod( header_tokens[0], &version_ );
            OPENGEODE_EXCEPTION( ok, "[MSHInput::set_msh_version] Error while "
                                     "reading file version" );
            OPENGEODE_EXCEPTION( version() == 2 || version() == 4,
                "[MSHInput::set_msh_version] Only MSH file format "
                "versions 2 and 4 are supported for now." );
            if( string_to_index( header_tokens[1] ) != 0 )
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
            const auto tokens = get_tokens( line );
            create_corners( string_to_index( tokens.at( 0 ) ) );
            create_lines( string_to_index( tokens.at( 1 ) ) );
            create_surfaces( string_to_index( tokens.at( 2 ) ) );
            create_blocks( string_to_index( tokens.at( 3 ) ) );
            check_keyword( "$EndEntities" );
        }

        void create_corners( const geode::index_t nb_corners )
        {
            for( const auto unused : geode::Range{ nb_corners } )
            {
                geode_unused( unused );
                std::string line;
                std::getline( file_, line );
                const auto tokens = get_tokens( line );
                const auto corner_uuid = builder_.add_corner();
                gmsh_id2uuids_
                    .elementary_ids[{ geode::Corner3D::component_type_static(),
                        string_to_index( tokens.at( 0 ) ) }] = corner_uuid;
            }
        }

        void create_lines( const geode::index_t nb_lines )
        {
            for( const auto unused : geode::Range{ nb_lines } )
            {
                geode_unused( unused );
                std::string line;
                std::getline( file_, line );
                const auto tokens = get_tokens( line );
                const auto line_uuid = builder_.add_line();
                gmsh_id2uuids_
                    .elementary_ids[{ geode::Line3D::component_type_static(),
                        string_to_index( tokens.at( 0 ) ) }] = line_uuid;
                // TODO physical tags
                const auto nb_physical_tags = string_to_index( tokens.at( 7 ) );
                for( const auto b : geode::Range{ string_to_index(
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
                const auto tokens = get_tokens( file_line );
                const auto surface_uuid = builder_.add_surface();
                const auto& surface = brep_.surface( surface_uuid );
                gmsh_id2uuids_
                    .elementary_ids[{ geode::Surface3D::component_type_static(),
                        string_to_index( tokens.at( 0 ) ) }] = surface_uuid;
                // TODO physical tags
                absl::flat_hash_map< geode::index_t, geode::index_t >
                    boundary_counter;
                const auto nb_physical_tags = string_to_index( tokens.at( 7 ) );
                for( const auto b : geode::Range{ string_to_index(
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
                const auto tokens = get_tokens( line );

                const auto block_uuid =
                    builder_.add_block( geode::MeshFactory::default_impl(
                        geode::HybridSolid3D::type_name_static() ) );
                gmsh_id2uuids_
                    .elementary_ids[{ geode::Block3D::component_type_static(),
                        string_to_index( tokens.at( 0 ) ) }] = block_uuid;
                // TODO physical tags
                const auto nb_physical_tags = string_to_index( tokens.at( 7 ) );
                for( const auto b : geode::Range{ string_to_index(
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

        geode::Point3D read_node_coordinates( absl::string_view x_str,
            absl::string_view y_str,
            absl::string_view z_str )
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
            const auto tokens = get_tokens( line );
            const auto nb_nodes = string_to_index( tokens.at( 0 ) );
            nodes_.resize( nb_nodes );
            for( const auto unused : geode::Range{ nb_nodes } )
            {
                geode_unused( unused );
                std::getline( file_, line );
                geode::index_t node_id;
                geode::Point3D node;
                std::tie( node_id, node ) = read_node( line );
                nodes_[node_id - OFFSET_START] = node;
            }
            check_keyword( "$EndNodes" );
            builder_.create_unique_vertices( nb_nodes );
        }

        std::tuple< geode::index_t, geode::Point3D > read_node(
            const std::string& line )
        {
            const auto tokens = get_tokens( line );
            return std::make_tuple( string_to_index( tokens.at( 0 ) ),
                read_node_coordinates(
                    tokens.at( 1 ), tokens.at( 2 ), tokens.at( 3 ) ) );
        }

        void read_node_section_v4()
        {
            go_to_section( "$Nodes" );
            std::string line;
            std::getline( file_, line );
            const auto tokens = get_tokens( line );
            const auto nb_total_nodes = string_to_index( tokens.at( 1 ) );
            const auto min_node_id = string_to_index( tokens.at( 2 ) );
            const auto max_node_id = string_to_index( tokens.at( 3 ) );
            OPENGEODE_EXCEPTION(
                min_node_id == 1 && max_node_id == nb_total_nodes,
                "[MSHInput::read_node_section_v4] Non continuous node indexing "
                "is not supported for now" );
            nodes_.resize( nb_total_nodes );
            for( const auto unused :
                geode::Range{ string_to_index( tokens.at( 0 ) ) } )
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
            const auto tokens = get_tokens( line );
            const auto nb_nodes = string_to_index( tokens.at( 3 ) );
            OPENGEODE_EXCEPTION( string_to_index( tokens.at( 2 ) ) == 0,
                "[MSHInput::read_node_group] Parametric node coordinates "
                "is not supported for now" );
            absl::FixedArray< geode::index_t > node_ids( nb_nodes );
            for( const auto n : geode::Range{ nb_nodes } )
            {
                std::getline( file_, line );
                const auto node_tokens = get_tokens( line );
                node_ids[n] = string_to_index( node_tokens.at( 0 ) );
            }
            for( const auto node_id : node_ids )
            {
                std::getline( file_, line );
                const auto node_tokens = get_tokens( line );
                nodes_[node_id - OFFSET_START] =
                    read_node_coordinates( node_tokens.at( 0 ),
                        node_tokens.at( 1 ), node_tokens.at( 2 ) );
            }
        }

        void read_element_section_v2()
        {
            go_to_section( "$Elements" );
            std::string line;
            std::getline( file_, line );
            const auto tokens = get_tokens( line );
            const auto nb_elements = string_to_index( tokens.at( 0 ) );
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
            const auto tokens = get_tokens( line );
            geode::index_t t{ 0 };
            OPENGEODE_EXCEPTION(
                expected_element_id == string_to_index( tokens.at( t++ ) ),
                "[MSHInput::read_element] Element indices should be "
                "continuous." );

            // Element type
            const auto mesh_element_type_id =
                string_to_index( tokens.at( t++ ) );
            // Tags
            const auto nb_tags = string_to_index( tokens.at( t++ ) );
            OPENGEODE_EXCEPTION( nb_tags >= 2, "[MSHInput::read_element] "
                                               "Number of tags for an element "
                                               "should be at least 2." );
            const auto physical_entity = string_to_index( tokens.at( t++ ) );
            const auto elementary_entity = string_to_index( tokens.at( t++ ) );
            t += nb_tags - 2;
            // TODO: create relation to the parent
            absl::Span< const absl::string_view > vertex_ids(
                &tokens[t], tokens.size() - t );

            const auto element =
                GMSHElementFactory::create( mesh_element_type_id,
                    physical_entity, elementary_entity, vertex_ids );
            element->add_element( brep_, gmsh_id2uuids_ );
        }

        void read_element_section_v4()
        {
            go_to_section( "$Elements" );
            std::string line;
            std::getline( file_, line );
            const auto tokens = get_tokens( line );
            const auto nb_total_elements = string_to_index( tokens.at( 1 ) );
            const auto min_element_id = string_to_index( tokens.at( 2 ) );
            const auto max_element_id = string_to_index( tokens.at( 3 ) );
            OPENGEODE_EXCEPTION(
                min_element_id == 1 && max_element_id == nb_total_elements,
                "[MSHInput::read_element_section_v4] Non continuous element "
                "indexing is not supported for now" );
            for( const auto unused :
                geode::Range{ string_to_index( tokens.at( 0 ) ) } )
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
            const auto tokens = get_tokens( line );
            const auto entity_id = string_to_index( tokens.at( 1 ) );
            const auto mesh_element_type_id = string_to_index( tokens.at( 2 ) );
            for( const auto unused :
                geode::Range{ string_to_index( tokens.at( 3 ) ) } )
            {
                geode_unused( unused );
                std::getline( file_, line );
                const auto line_tokens = get_tokens( line );
                absl::Span< const absl::string_view > vertex_ids(
                    &line_tokens[1], line_tokens.size() - 1 );
                constexpr geode::index_t physical_entity{ 0 };
                const auto element =
                    GMSHElementFactory::create( mesh_element_type_id,
                        physical_entity, entity_id, vertex_ids );
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
            for( const auto& s : brep_.surfaces() )
            {
                filter_duplicated_surface_vertices( s, brep_ );
                auto surface_builder = builder_.surface_mesh_builder( s.id() );
                const auto& mesh = s.mesh();
                for( const auto v : geode::Range{ mesh.nb_vertices() } )
                {
                    surface_builder->set_point(
                        v, nodes_[brep_.unique_vertex(
                               { s.component_id(), v } )] );
                }
                surface_builder->compute_polygon_adjacencies();
                std::vector< geode::PolygonEdge > polygon_edges;
                for( const auto& line : brep_.internal_lines( s ) )
                {
                    const auto& edges = line.mesh();
                    for( const auto e : geode::Range{ edges.nb_edges() } )
                    {
                        const auto e0 = edges.edge_vertex( { e, 0 } );
                        const auto e1 = edges.edge_vertex( { e, 1 } );
                        const auto surface0 = brep_.component_mesh_vertices(
                            brep_.unique_vertex( { line.component_id(), e0 } ),
                            s.id() );
                        const auto surface1 = brep_.component_mesh_vertices(
                            brep_.unique_vertex( { line.component_id(), e1 } ),
                            s.id() );
                        for( const auto v0 : surface0 )
                        {
                            for( const auto v1 : surface1 )
                            {
                                if( const auto edge0 =
                                        mesh.polygon_edge_from_vertices(
                                            v0, v1 ) )
                                {
                                    polygon_edges.emplace_back( edge0.value() );
                                }
                                if( const auto edge1 =
                                        mesh.polygon_edge_from_vertices(
                                            v1, v0 ) )
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
        BRep MSHInput::read()
        {
            BRep brep;
            MSHInputImpl impl( filename(), brep );
            impl.read_file();
            impl.build_geometry();
            impl.build_topology();
            return brep;
        }
    } // namespace detail
} // namespace geode
