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

#include <geode/io/model/internal/msh_input.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <optional>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_split.h>

#include <geode/basic/file.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/string.hpp>
#include <geode/basic/uuid.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/edged_curve_builder.hpp>
#include <geode/mesh/builder/point_set_builder.hpp>
#include <geode/mesh/builder/solid_mesh_builder.hpp>
#include <geode/mesh/builder/surface_mesh_builder.hpp>
#include <geode/mesh/core/edged_curve.hpp>
#include <geode/mesh/core/hybrid_solid.hpp>
#include <geode/mesh/core/mesh_factory.hpp>
#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/core/solid_mesh.hpp>
#include <geode/mesh/core/surface_mesh.hpp>

#include <geode/model/helpers/detail/build_model_boundaries.hpp>
#include <geode/model/mixin/core/block.hpp>
#include <geode/model/mixin/core/corner.hpp>
#include <geode/model/mixin/core/line.hpp>
#include <geode/model/mixin/core/surface.hpp>
#include <geode/model/representation/builder/brep_builder.hpp>
#include <geode/model/representation/core/brep.hpp>

#include <geode/io/model/common.hpp>
#include <geode/io/model/internal/msh_common.hpp>

namespace
{
    /// Counts read from the file are not trusted to reserve memory
    constexpr geode::index_t MAX_RESERVE{ 1U << 24U };

    using Facets = std::vector< std::vector< geode::local_index_t > >;

    struct GmshElementType
    {
        geode::local_index_t dimension;
        geode::local_index_t nb_vertices;
        const Facets* facets{ nullptr };
    };

    const Facets& tetrahedron_facets()
    {
        static const Facets facets{ { 0, 1, 2 }, { 0, 2, 3 }, { 1, 3, 2 },
            { 0, 3, 1 } };
        return facets;
    }

    const Facets& hexahedron_facets()
    {
        static const Facets facets{ { 0, 1, 2, 3 }, { 7, 6, 5, 4 },
            { 0, 3, 7, 4 }, { 1, 5, 6, 2 }, { 2, 6, 7, 3 }, { 0, 4, 5, 1 } };
        return facets;
    }

    const Facets& prism_facets()
    {
        static const Facets facets{ { 0, 1, 2 }, { 5, 4, 3 }, { 0, 2, 5, 3 },
            { 0, 3, 4, 1 }, { 1, 4, 5, 2 } };
        return facets;
    }

    const Facets& pyramid_facets()
    {
        static const Facets facets{ { 0, 3, 4 }, { 0, 4, 1 }, { 4, 3, 2 },
            { 1, 4, 2 }, { 0, 1, 2, 3 } };
        return facets;
    }

    // NOLINTBEGIN(*-magic-numbers)
    GmshElementType gmsh_element_type( geode::index_t gmsh_type )
    {
        switch( gmsh_type )
        {
            case 15:
                return { 0, 1 };
            case 1:
                return { 1, 2 };
            case 2:
                return { 2, 3 };
            case 3:
                return { 2, 4 };
            case 4:
                return { 3, 4, &tetrahedron_facets() };
            case 5:
                return { 3, 8, &hexahedron_facets() };
            case 6:
                return { 3, 6, &prism_facets() };
            case 7:
                return { 3, 5, &pyramid_facets() };
            default:
                throw geode::OpenGeodeIOModelException{ nullptr,
                    geode::OpenGeodeException::TYPE::data,
                    "[MSHInput] Unsupported GMSH element type: ", gmsh_type };
        }
    }
    // NOLINTEND(*-magic-numbers)

    struct ComponentElements
    {
        geode::uuid id;
        /// Gmsh type of each element
        std::vector< geode::index_t > types;
        /// Node indices of all elements, element after element
        std::vector< geode::index_t > nodes;
    };

    /*!
     * Maps the GMSH node tags to the node indices.
     * Tags 1 to n in order (the common case) need no storage.
     */
    class NodeTags
    {
    public:
        void add( geode::index_t tag, geode::index_t node )
        {
            if( contiguous_
                && tag == node + geode::internal::GMSH_OFFSET_START )
            {
                nb_contiguous_++;
                return;
            }
            if( contiguous_ )
            {
                contiguous_ = false;
                tag2node_.reserve( nb_contiguous_ + 1 );
                for( const auto n : geode::Range{ nb_contiguous_ } )
                {
                    tag2node_.emplace(
                        n + geode::internal::GMSH_OFFSET_START, n );
                }
            }
            const auto inserted = tag2node_.emplace( tag, node ).second;
            geode::OpenGeodeIOModelException::check_exception( inserted,
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[MSHInput] Node tag ", tag, " is defined twice" );
        }

        std::optional< geode::index_t > node( geode::index_t tag ) const
        {
            if( contiguous_ )
            {
                if( tag < geode::internal::GMSH_OFFSET_START
                    || tag - geode::internal::GMSH_OFFSET_START
                           >= nb_contiguous_ )
                {
                    return std::nullopt;
                }
                return tag - geode::internal::GMSH_OFFSET_START;
            }
            const auto it = tag2node_.find( tag );
            if( it == tag2node_.end() )
            {
                return std::nullopt;
            }
            return it->second;
        }

    private:
        bool contiguous_{ true };
        geode::index_t nb_contiguous_{ 0 };
        absl::flat_hash_map< geode::index_t, geode::index_t > tag2node_;
    };

    class MSHInputImpl
    {
        static constexpr geode::local_index_t NB_DIMENSIONS{ 4 };

    public:
        MSHInputImpl( std::string_view filename, geode::BRep& brep )
            : file_{ geode::to_string( filename ) },
              brep_( brep ),
              builder_{ brep }
        {
            geode::OpenGeodeIOModelException::check_exception( file_.good(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[MSHInput] Error while opening file: ", filename );
        }

        void read_header()
        {
            check_keyword( "$MeshFormat" );
            std::string line;
            read_line( line );
            set_msh_version( line );
            check_keyword( "$EndMeshFormat" );
        }

        void read_file()
        {
            read_header();
            bool nodes_read{ false };
            bool elements_read{ false };
            std::string line;
            while( next_line( line ) )
            {
                absl::StripAsciiWhitespace( &line );
                if( line == "$Entities" && version() == 4 )
                {
                    has_entities_ = true;
                    read_entity_section();
                }
                else if( line == "$Nodes" )
                {
                    if( version() == 2 )
                    {
                        read_node_section_v2();
                    }
                    else
                    {
                        read_node_section_v4();
                    }
                    nodes_read = true;
                }
                else if( line == "$Elements" )
                {
                    if( version() == 2 )
                    {
                        read_element_section_v2();
                    }
                    else
                    {
                        read_element_section_v4();
                    }
                    elements_read = true;
                }
                else if( geode::string_starts_with( line, "$" ) )
                {
                    skip_section( line );
                }
            }
            geode::OpenGeodeIOModelException::check_exception(
                nodes_read && elements_read, nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[MSHInput::read_file] Missing $Nodes or $Elements section" );
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
            if( has_entities_ )
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
                const auto cmv = get_component_mesh_vertices( uv );
                filter_potential_relationships( std::get< 0 >( cmv ),
                    std::get< 1 >( cmv ), corner_line_relations );
                filter_potential_relationships( std::get< 1 >( cmv ),
                    std::get< 2 >( cmv ), line_surface_relations );
                filter_potential_relationships( std::get< 2 >( cmv ),
                    std::get< 3 >( cmv ), surface_block_relations );
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
        /// MSH 4.0 differs from 4.1 in the node and element block layouts
        bool is_v40() const
        {
            return version() == 4 && version_ < 4.05; // NOLINT
        }

        geode::index_t version() const
        {
            return static_cast< geode::index_t >( std::floor( version_ ) );
        }

        void check_keyword( std::string_view keyword )
        {
            std::string line;
            read_line( line );
            absl::StripAsciiWhitespace( &line );
            geode::OpenGeodeIOModelException::check_exception(
                geode::string_starts_with( line, keyword ), nullptr,
                geode::OpenGeodeException::TYPE::data, "[MSHInput] Line ",
                line_number_, ": expected ", keyword, " but got \"", line,
                "\"" );
        }

        bool next_line( std::string& line )
        {
            if( !std::getline( file_, line ) )
            {
                return false;
            }
            line_number_++;
            return true;
        }

        void read_line( std::string& line )
        {
            geode::OpenGeodeIOModelException::check_exception(
                next_line( line ), nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[MSHInput] Unexpected end of file after line ", line_number_ );
        }

        std::string_view token( absl::Span< const std::string_view > tokens,
            std::size_t index ) const
        {
            geode::OpenGeodeIOModelException::check_exception(
                index < tokens.size(), nullptr,
                geode::OpenGeodeException::TYPE::data, "[MSHInput] Line ",
                line_number_, ": missing value (expected at least ", index + 1,
                " values, got ", tokens.size(), ")" );
            return tokens[index];
        }

        void skip_section( std::string_view section_header )
        {
            const auto end_section =
                absl::StrCat( "$End", section_header.substr( 1 ) );
            std::string line;
            while( next_line( line ) )
            {
                if( geode::string_starts_with( line, end_section ) )
                {
                    return;
                }
            }
            throw geode::OpenGeodeIOModelException{ nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[MSHInput::skip_section] Cannot find the end of section ",
                section_header };
        }

        void set_msh_version( const std::string& line )
        {
            const auto header_tokens = geode::string_split( line );
            version_ = geode::string_to_double( token( header_tokens, 0 ) );
            geode::OpenGeodeIOModelException::check_exception(
                version() == 2 || version() == 4, nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[MSHInput::set_msh_version] Only MSH file format "
                "versions 2 and 4 are supported for now." );
            if( geode::string_to_index( token( header_tokens, 1 ) ) != 0 )
            {
                throw geode::OpenGeodeIOModelException{ nullptr,
                    geode::OpenGeodeException::TYPE::internal,
                    "[MSHInput::set_msh_version] Binary format is not "
                    "supported for now." };
            }
        }

        absl::Span< const std::string_view > tokenize( std::string_view line )
        {
            tokens_.clear();
            for( const auto token : absl::StrSplit(
                     line, absl::ByAnyChar( " \t\r" ), absl::SkipEmpty() ) )
            {
                tokens_.push_back( token );
            }
            return tokens_;
        }

        ComponentElements& component_elements(
            geode::local_index_t dimension, geode::index_t entity_tag )
        {
            auto [it, inserted] =
                components_[dimension].try_emplace( entity_tag );
            if( inserted )
            {
                it->second.id = create_component( dimension );
            }
            return it->second;
        }

        const geode::uuid& component_id(
            geode::local_index_t dimension, geode::index_t entity_tag ) const
        {
            return components_[dimension].at( entity_tag ).id;
        }

        geode::uuid create_component( geode::local_index_t dimension )
        {
            if( dimension == 0 )
            {
                return builder_.add_corner();
            }
            if( dimension == 1 )
            {
                return builder_.add_line();
            }
            if( dimension == 2 )
            {
                return builder_.add_surface();
            }
            return builder_.add_block( geode::MeshFactory::default_impl(
                geode::HybridSolid3D::type_name_static() ) );
        }

        void read_entity_section()
        {
            std::string line;
            read_line( line );
            const auto tokens = geode::string_split( line );
            create_corners( geode::string_to_index( token( tokens, 0 ) ) );
            create_lines( geode::string_to_index( token( tokens, 1 ) ) );
            create_surfaces( geode::string_to_index( token( tokens, 2 ) ) );
            create_blocks( geode::string_to_index( token( tokens, 3 ) ) );
            check_keyword( "$EndEntities" );
        }

        void create_corners( const geode::index_t nb_corners )
        {
            for( const auto unused : geode::Range{ nb_corners } )
            {
                geode_unused( unused );
                std::string line;
                read_line( line );
                const auto tokens = geode::string_split( line );
                component_elements(
                    0, geode::string_to_index( token( tokens, 0 ) ) );
            }
        }

        void create_lines( const geode::index_t nb_lines )
        {
            for( const auto unused : geode::Range{ nb_lines } )
            {
                geode_unused( unused );
                std::string line;
                read_line( line );
                const auto tokens = geode::string_split( line );
                const auto& line_uuid = component_elements(
                    1, geode::string_to_index( token( tokens, 0 ) ) )
                                            .id;
                // TODO physical tags
                const auto nb_physical_tags =
                    geode::string_to_index( token( tokens, 7 ) );
                for( const auto b : geode::Range{ geode::string_to_index(
                         token( tokens, 8 + nb_physical_tags ) ) } )
                {
                    const auto boundary_msh_id =
                        std::abs( geode::string_to_double(
                            token( tokens, 9 + nb_physical_tags + b ) ) );
                    builder_.add_corner_line_boundary_relationship(
                        brep_.corner(
                            component_id( 0, static_cast< geode::index_t >(
                                                 boundary_msh_id ) ) ),
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
                read_line( file_line );
                const auto tokens = geode::string_split( file_line );
                const auto& surface_uuid = component_elements(
                    2, geode::string_to_index( token( tokens, 0 ) ) )
                                               .id;
                const auto& surface = brep_.surface( surface_uuid );
                // TODO physical tags
                absl::flat_hash_map< geode::index_t, geode::index_t >
                    boundary_counter;
                const auto nb_physical_tags =
                    geode::string_to_index( token( tokens, 7 ) );
                for( const auto b : geode::Range{ geode::string_to_index(
                         token( tokens, 8 + nb_physical_tags ) ) } )
                {
                    const auto boundary_msh_id = geode::string_to_double(
                        token( tokens, 9 + nb_physical_tags + b ) );
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
                        brep_.line( component_id( 1, boundary.first ) );
                    if( boundary.second == 1 )
                    {
                        builder_.add_line_surface_boundary_relationship(
                            line, surface );
                    }
                    else
                    {
                        geode::OpenGeodeIOModelException::check_exception(
                            boundary.second == 2, nullptr,
                            geode::OpenGeodeException::TYPE::data,
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
                read_line( line );
                const auto tokens = geode::string_split( line );
                const auto& block_uuid = component_elements(
                    3, geode::string_to_index( token( tokens, 0 ) ) )
                                             .id;
                // TODO physical tags
                const auto nb_physical_tags =
                    geode::string_to_index( token( tokens, 7 ) );
                for( const auto b : geode::Range{ geode::string_to_index(
                         token( tokens, 8 + nb_physical_tags ) ) } )
                {
                    const auto boundary_msh_id = std::abs( geode::string_to_int(
                        token( tokens, 9 + nb_physical_tags + b ) ) );
                    builder_.add_surface_block_boundary_relationship(
                        brep_.surface(
                            component_id( 2, static_cast< geode::index_t >(
                                                 boundary_msh_id ) ) ),
                        brep_.block( block_uuid ) );
                }
            }
        }

        geode::Point3D read_node_coordinates( std::string_view x_str,
            std::string_view y_str,
            std::string_view z_str )
        {
            return geode::Point3D{ { geode::string_to_double( x_str ),
                geode::string_to_double( y_str ),
                geode::string_to_double( z_str ) } };
        }

        void reserve_nodes( geode::index_t nb_nodes )
        {
            nodes_.reserve( std::min( nb_nodes, MAX_RESERVE ) );
        }

        void add_node( geode::index_t tag, const geode::Point3D& point )
        {
            node_tags_.add( tag, nodes_.size() );
            nodes_.push_back( point );
        }

        void read_node_section_v2()
        {
            std::string line;
            read_line( line );
            const auto tokens = geode::string_split( line );
            const auto nb_nodes = geode::string_to_index( token( tokens, 0 ) );
            reserve_nodes( nb_nodes );
            for( const auto unused : geode::Range{ nb_nodes } )
            {
                geode_unused( unused );
                read_line( line );
                const auto node_tokens = tokenize( line );
                add_node( geode::string_to_index( token( node_tokens, 0 ) ),
                    read_node_coordinates( token( node_tokens, 1 ),
                        token( node_tokens, 2 ), token( node_tokens, 3 ) ) );
            }
            check_keyword( "$EndNodes" );
            builder_.create_unique_vertices( nodes_.size() );
        }

        void read_node_section_v4()
        {
            std::string line;
            read_line( line );
            const auto tokens = geode::string_split( line );
            reserve_nodes( geode::string_to_index( token( tokens, 1 ) ) );
            for( const auto unused :
                geode::Range{ geode::string_to_index( token( tokens, 0 ) ) } )
            {
                geode_unused( unused );
                read_node_group();
            }
            check_keyword( "$EndNodes" );
            builder_.create_unique_vertices( nodes_.size() );
        }

        void read_node_group()
        {
            std::string line;
            read_line( line );
            const auto tokens = geode::string_split( line );
            const auto nb_nodes = geode::string_to_index( token( tokens, 3 ) );
            geode::OpenGeodeIOModelException::check_exception(
                geode::string_to_index( token( tokens, 2 ) ) == 0, nullptr,
                geode::OpenGeodeException::TYPE::internal,
                "[MSHInput::read_node_group] Parametric node coordinates "
                "is not supported for now" );
            std::vector< geode::index_t > node_tags;
            node_tags.reserve( std::min( nb_nodes, MAX_RESERVE ) );
            if( is_v40() )
            {
                // MSH 4.0: "tag x y z" on each line
                for( const auto n : geode::Range{ nb_nodes } )
                {
                    geode_unused( n );
                    read_line( line );
                    const auto node_tokens = tokenize( line );
                    add_node( geode::string_to_index( token( node_tokens, 0 ) ),
                        read_node_coordinates( token( node_tokens, 1 ),
                            token( node_tokens, 2 ),
                            token( node_tokens, 3 ) ) );
                }
                return;
            }
            // MSH 4.1: all the tags, then all the coordinates
            for( const auto n : geode::Range{ nb_nodes } )
            {
                geode_unused( n );
                read_line( line );
                const auto node_tokens = tokenize( line );
                node_tags.push_back(
                    geode::string_to_index( token( node_tokens, 0 ) ) );
            }
            for( const auto node_tag : node_tags )
            {
                read_line( line );
                const auto node_tokens = tokenize( line );
                add_node( node_tag,
                    read_node_coordinates( token( node_tokens, 0 ),
                        token( node_tokens, 1 ), token( node_tokens, 2 ) ) );
            }
        }

        void read_element_section_v2()
        {
            std::string line;
            read_line( line );
            const auto tokens = geode::string_split( line );
            const auto nb_elements =
                geode::string_to_index( token( tokens, 0 ) );
            for( const auto unused : geode::Range{ nb_elements } )
            {
                geode_unused( unused );
                read_line( line );
                read_element( line );
            }
            check_keyword( "$EndElements" );
        }

        void add_element( ComponentElements& component,
            geode::index_t gmsh_type,
            absl::Span< const std::string_view > node_tokens )
        {
            component.types.push_back( gmsh_type );
            for( const auto& node : node_tokens )
            {
                const auto node_tag = geode::string_to_index( node );
                const auto node_id = node_tags_.node( node_tag );
                geode::OpenGeodeIOModelException::check_exception(
                    node_id.has_value(), nullptr,
                    geode::OpenGeodeException::TYPE::data, "[MSHInput] Line ",
                    line_number_, ": element refers to unknown node ",
                    node_tag );
                component.nodes.push_back( node_id.value() );
            }
        }

        void read_element( const std::string& line )
        {
            const auto tokens = tokenize( line );
            // Element tags are not used: they do not need to be continuous
            geode::index_t t{ 1 };
            // Element type
            const auto mesh_element_type_id =
                geode::string_to_index( token( tokens, t++ ) );
            const auto element_type = gmsh_element_type( mesh_element_type_id );
            // Tags
            const auto nb_tags = geode::string_to_index( token( tokens, t++ ) );
            geode::OpenGeodeIOModelException::check_exception( nb_tags >= 2,
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[MSHInput::read_element] Number of tags for an element should "
                "be at least 2." );
            geode::OpenGeodeIOModelException::check_exception(
                nb_tags <= tokens.size(), nullptr,
                geode::OpenGeodeException::TYPE::data, "[MSHInput] Line ",
                line_number_, ": wrong number of element tags" );
            t++; // physical entity
            const auto elementary_entity =
                geode::string_to_index( token( tokens, t++ ) );
            geode::OpenGeodeIOModelException::check_exception(
                elementary_entity > 0, nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[MSHInput::read_element] GMSH tag for elementary entity "
                "(second tag) should not be null" );
            t += nb_tags - 2;
            token( tokens, t + element_type.nb_vertices - 1 );
            add_element(
                component_elements( element_type.dimension, elementary_entity ),
                mesh_element_type_id,
                tokens.subspan( t, element_type.nb_vertices ) );
        }

        void read_element_section_v4()
        {
            std::string line;
            read_line( line );
            const auto tokens = geode::string_split( line );
            for( const auto unused :
                geode::Range{ geode::string_to_index( token( tokens, 0 ) ) } )
            {
                geode_unused( unused );
                read_element_group();
            }
            check_keyword( "$EndElements" );
        }

        void read_element_group()
        {
            std::string line;
            read_line( line );
            const auto tokens = geode::string_split( line );
            // MSH 4.0: tagEntity dimEntity typeEle numElements
            // MSH 4.1: entityDim entityTag elementType numElementsInBlock
            const auto entity_id =
                geode::string_to_index( token( tokens, is_v40() ? 0 : 1 ) );
            const auto mesh_element_type_id =
                geode::string_to_index( token( tokens, 2 ) );
            const auto element_type = gmsh_element_type( mesh_element_type_id );
            auto& component =
                component_elements( element_type.dimension, entity_id );
            const auto nb_elements =
                geode::string_to_index( token( tokens, 3 ) );
            component.types.reserve( component.types.size() + nb_elements );
            component.nodes.reserve( component.nodes.size()
                                     + nb_elements * element_type.nb_vertices );
            for( const auto unused : geode::Range{ nb_elements } )
            {
                geode_unused( unused );
                read_line( line );
                const auto line_tokens = tokenize( line );
                token( line_tokens, element_type.nb_vertices );
                add_element( component, mesh_element_type_id,
                    line_tokens.subspan( 1, element_type.nb_vertices ) );
            }
        }

        /*!
         * Creates the component mesh vertices: one vertex per distinct node,
         * linked to its unique vertex.
         * @return the component vertex of each element node
         */
        template < typename Component, typename MeshBuilder >
        std::vector< geode::index_t > create_component_vertices(
            const Component& component,
            MeshBuilder& mesh_builder,
            const ComponentElements& elements )
        {
            absl::flat_hash_map< geode::index_t, geode::index_t > node2vertex;
            std::vector< geode::index_t > vertex2node;
            std::vector< geode::index_t > element_vertices;
            element_vertices.reserve( elements.nodes.size() );
            for( const auto node : elements.nodes )
            {
                const auto [it, inserted] =
                    node2vertex.try_emplace( node, vertex2node.size() );
                if( inserted )
                {
                    vertex2node.push_back( node );
                }
                element_vertices.push_back( it->second );
            }
            const auto first_vertex =
                mesh_builder.create_vertices( vertex2node.size() );
            for( const auto v : geode::Indices{ vertex2node } )
            {
                const auto vertex = first_vertex + v;
                mesh_builder.set_point( vertex, nodes_[vertex2node[v]] );
                builder_.set_unique_vertex(
                    { component.component_id(), vertex }, vertex2node[v] );
            }
            for( auto& vertex : element_vertices )
            {
                vertex += first_vertex;
            }
            return element_vertices;
        }

        void build_corners()
        {
            for( const auto& [tag, elements] : components_[0] )
            {
                const auto& corner = brep_.corner( elements.id );
                auto mesh_builder = builder_.corner_mesh_builder( corner );
                create_component_vertices( corner, *mesh_builder, elements );
            }
        }

        void build_lines()
        {
            for( const auto& [tag, elements] : components_[1] )
            {
                const auto& line = brep_.line( elements.id );
                auto mesh_builder = builder_.line_mesh_builder( line );
                const auto vertices =
                    create_component_vertices( line, *mesh_builder, elements );
                for( geode::index_t v = 0; v + 1 < vertices.size(); v += 2 )
                {
                    mesh_builder->create_edge( vertices[v], vertices[v + 1] );
                }
            }
        }

        void build_surfaces()
        {
            for( const auto& [tag, elements] : components_[2] )
            {
                const auto& surface = brep_.surface( elements.id );
                auto mesh_builder = builder_.surface_mesh_builder( surface );
                const auto vertices = create_component_vertices(
                    surface, *mesh_builder, elements );
                absl::Span< const geode::index_t > remaining{ vertices };
                for( const auto type : elements.types )
                {
                    const auto nb_vertices =
                        gmsh_element_type( type ).nb_vertices;
                    mesh_builder->create_polygon(
                        remaining.subspan( 0, nb_vertices ) );
                    remaining.remove_prefix( nb_vertices );
                }
                mesh_builder->compute_polygon_adjacencies();
                unset_internal_line_adjacencies( surface, *mesh_builder );
            }
        }

        void unset_internal_line_adjacencies( const geode::Surface3D& surface,
            geode::SurfaceMeshBuilder3D& mesh_builder )
        {
            const auto& mesh = surface.mesh();
            std::vector< geode::PolygonEdge > polygon_edges;
            for( const auto& line : brep_.internal_lines( surface ) )
            {
                const auto& edges = line.mesh();
                for( const auto edge_id : geode::Range{ edges.nb_edges() } )
                {
                    const auto e0 = edges.edge_vertex( { edge_id, 0 } );
                    const auto e1 = edges.edge_vertex( { edge_id, 1 } );
                    const auto cmvs0 = brep_.component_mesh_vertices(
                        brep_.unique_vertex( { line.component_id(), e0 } ) );
                    const auto cmvs1 = brep_.component_mesh_vertices(
                        brep_.unique_vertex( { line.component_id(), e1 } ) );
                    for( const auto& cmv0 : cmvs0 )
                    {
                        if( cmv0.component_id.id != surface.id() )
                        {
                            continue;
                        }
                        for( const auto& cmv1 : cmvs1 )
                        {
                            if( cmv1.component_id.id != surface.id() )
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
                mesh_builder.unset_polygon_adjacent( edge );
            }
        }

        void build_blocks()
        {
            for( const auto& [tag, elements] : components_[3] )
            {
                const auto& block = brep_.block( elements.id );
                auto mesh_builder = builder_.block_mesh_builder( block );
                const auto vertices =
                    create_component_vertices( block, *mesh_builder, elements );
                absl::Span< const geode::index_t > remaining{ vertices };
                for( const auto type : elements.types )
                {
                    const auto element_type = gmsh_element_type( type );
                    mesh_builder->create_polyhedron(
                        remaining.subspan( 0, element_type.nb_vertices ),
                        *element_type.facets );
                    remaining.remove_prefix( element_type.nb_vertices );
                }
                mesh_builder->compute_polyhedron_adjacencies();
            }
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
                    b2i_relations[boundary_vertex.component_id.id].emplace(
                        incidence_vertex.component_id.id );
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
                if( b2i_relations.find( boundary_vertex.component_id.id )
                    == b2i_relations.end() )
                {
                    continue;
                }
                auto& incidences_in_relations =
                    b2i_relations.at( boundary_vertex.component_id.id );

                auto it = incidences_in_relations.cbegin();
                while( it != incidences_in_relations.cend() )
                {
                    const auto cur_it = it++;
                    const auto& incidence_id = *cur_it;
                    if( std::find_if( incidence_type_vertices.begin(),
                            incidence_type_vertices.end(),
                            [&incidence_id](
                                const geode::ComponentMeshVertex& cmv ) {
                                return cmv.component_id.id == incidence_id;
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
            for( const auto& cmv : brep_.component_mesh_vertices( uv ) )
            {
                if( cmv.component_id.type
                    == geode::Corner3D::component_type_static() )
                {
                    corners_vertices.push_back( cmv );
                    continue;
                }
                if( cmv.component_id.type
                    == geode::Line3D::component_type_static() )
                {
                    lines_vertices.push_back( cmv );
                    continue;
                }
                if( cmv.component_id.type
                    == geode::Surface3D::component_type_static() )
                {
                    surfaces_vertices.push_back( cmv );
                    continue;
                }
                if( cmv.component_id.type
                    == geode::Block3D::component_type_static() )
                {
                    blocks_vertices.push_back( cmv );
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
        double version_{ 2 };
        bool has_entities_{ false };
        geode::index_t line_number_{ 0 };
        std::vector< geode::Point3D > nodes_;
        NodeTags node_tags_;
        std::array< absl::flat_hash_map< geode::index_t, ComponentElements >,
            NB_DIMENSIONS >
            components_;
        std::vector< std::string_view > tokens_;
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

        Percentage MSHInput::is_loadable() const
        {
            BRep brep;
            try
            {
                MSHInputImpl impl( filename(), brep );
                impl.read_header();
            }
            catch( const OpenGeodeException& )
            {
                return Percentage{ 0 };
            }
            return Percentage{ 1 };
        }
    } // namespace internal
} // namespace geode
