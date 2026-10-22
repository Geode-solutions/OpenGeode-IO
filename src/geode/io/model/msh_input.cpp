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
#include <limits>
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

#include <geode/geometry/distance.hpp>
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
    constexpr geode::index_t POINT_TYPE{ 15 };

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
            case POINT_TYPE:
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
        /// Point given in the $Entities section (corners only)
        std::optional< geode::Point3D > position;
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
            complete_corners();
            remove_empty_components();
            build_corners();
            build_lines();
            build_surfaces();
            build_blocks();
        }

        void build_topology()
        {
            infer_corner_relations();
            infer_line_relations();
            infer_surface_relations();
            if( !has_entities_ )
            {
                geode::detail::build_model_boundaries( brep_, builder_ );
            }
            unset_internal_line_adjacencies();
            unset_internal_surface_adjacencies();
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
                auto& corner = component_elements(
                    0, geode::string_to_index( token( tokens, 0 ) ) );
                // MSH 4.1: tag X Y Z, MSH 4.0: tag minX minY minZ maxX...
                corner.position = read_node_coordinates( token( tokens, 1 ),
                    token( tokens, 2 ), token( tokens, 3 ) );
            }
        }

        /*!
         * Reads the bounding entities of a curve, surface or volume entity
         * line: tag box(6) numPhysicalTags physicalTags... numBounding
         * boundingTags... Unknown bounding entities are ignored: Gmsh may
         * write invalid tags for embedded entities.
         */
        std::vector< geode::uuid > read_boundaries(
            absl::Span< const std::string_view > tokens,
            geode::local_index_t boundary_dimension )
        {
            const auto nb_physical_tags =
                geode::string_to_index( token( tokens, 7 ) );
            const auto nb_boundaries =
                geode::string_to_index( token( tokens, 8 + nb_physical_tags ) );
            std::vector< geode::uuid > boundaries;
            for( const auto b : geode::Range{ nb_boundaries } )
            {
                const auto boundary_tag = static_cast< geode::index_t >(
                    std::abs( geode::string_to_int(
                        token( tokens, 9 + nb_physical_tags + b ) ) ) );
                const auto it =
                    components_[boundary_dimension].find( boundary_tag );
                if( it == components_[boundary_dimension].end() )
                {
                    geode::Logger::warning( "[MSHInput] Line ", line_number_,
                        ": unknown bounding entity ", boundary_tag,
                        " of dimension ", boundary_dimension, " is ignored" );
                    continue;
                }
                boundaries.push_back( it->second.id );
            }
            return boundaries;
        }

        void create_lines( const geode::index_t nb_lines )
        {
            for( const auto unused : geode::Range{ nb_lines } )
            {
                geode_unused( unused );
                std::string file_line;
                read_line( file_line );
                const auto tokens = geode::string_split( file_line );
                const auto& line = brep_.line( component_elements(
                    1, geode::string_to_index( token( tokens, 0 ) ) )
                        .id );
                for( const auto& corner_id : read_boundaries( tokens, 0 ) )
                {
                    builder_.add_corner_line_boundary_relationship(
                        brep_.corner( corner_id ), line );
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
                const auto& surface = brep_.surface( component_elements(
                    2, geode::string_to_index( token( tokens, 0 ) ) )
                        .id );
                absl::flat_hash_map< geode::uuid, geode::index_t >
                    boundary_counter;
                for( const auto& line_id : read_boundaries( tokens, 1 ) )
                {
                    boundary_counter[line_id]++;
                }
                for( const auto& [line_id, count] : boundary_counter )
                {
                    const auto& line = brep_.line( line_id );
                    if( count == 1 )
                    {
                        builder_.add_line_surface_boundary_relationship(
                            line, surface );
                    }
                    else
                    {
                        // A line bounding the surface on both sides
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
                std::string file_line;
                read_line( file_line );
                const auto tokens = geode::string_split( file_line );
                const auto& block = brep_.block( component_elements(
                    3, geode::string_to_index( token( tokens, 0 ) ) )
                        .id );
                absl::flat_hash_map< geode::uuid, geode::index_t >
                    boundary_counter;
                for( const auto& surface_id : read_boundaries( tokens, 2 ) )
                {
                    boundary_counter[surface_id]++;
                }
                for( const auto& [surface_id, count] : boundary_counter )
                {
                    const auto& surface = brep_.surface( surface_id );
                    if( count == 1 )
                    {
                        builder_.add_surface_block_boundary_relationship(
                            surface, block );
                    }
                    else
                    {
                        // A surface bounding the block on both sides
                        builder_.add_surface_block_internal_relationship(
                            surface, block );
                    }
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
            // Parametric coordinates (tokens[2] == 1) follow x y z: ignored
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

        /*!
         * Gmsh does not save the point elements of a model with physical
         * groups if no physical group contains points. The corner node is
         * then recovered from the incident lines, as the node closest to the
         * point given in the $Entities section.
         */
        void complete_corners()
        {
            absl::flat_hash_map< geode::uuid, const ComponentElements* > lines;
            for( const auto& [tag, elements] : components_[1] )
            {
                lines.emplace( elements.id, &elements );
            }
            for( auto& [tag, corner] : components_[0] )
            {
                if( !corner.nodes.empty() || !corner.position )
                {
                    continue;
                }
                std::optional< geode::index_t > closest_node;
                auto min_distance = std::numeric_limits< double >::max();
                for( const auto& line :
                    brep_.incidences( brep_.corner( corner.id ) ) )
                {
                    for( const auto node : lines.at( line.id() )->nodes )
                    {
                        const auto distance = geode::point_point_distance(
                            nodes_[node], corner.position.value() );
                        if( distance < min_distance )
                        {
                            min_distance = distance;
                            closest_node = node;
                        }
                    }
                }
                if( closest_node )
                {
                    corner.types.push_back( POINT_TYPE );
                    corner.nodes.push_back( closest_node.value() );
                }
            }
        }

        template < typename Component >
        void remove_empty_components( geode::local_index_t dimension,
            std::string_view name,
            void ( geode::BRepBuilder::*remove )( const Component& ),
            const Component& ( geode::BRep::*component )( const geode::uuid& )
                const )
        {
            std::vector< geode::index_t > empty_tags;
            for( const auto& [tag, elements] : components_[dimension] )
            {
                if( elements.nodes.empty() )
                {
                    geode::Logger::warning( "[MSHInput] ", name, " ", tag,
                        " has no element, it is removed" );
                    ( builder_.*remove )( ( brep_.*component )( elements.id ) );
                    empty_tags.push_back( tag );
                }
            }
            for( const auto tag : empty_tags )
            {
                components_[dimension].erase( tag );
            }
        }

        void remove_empty_components()
        {
            remove_empty_components< geode::Corner3D >( 0, "Point",
                &geode::BRepBuilder::remove_corner, &geode::BRep::corner );
            remove_empty_components< geode::Line3D >( 1, "Curve",
                &geode::BRepBuilder::remove_line, &geode::BRep::line );
            remove_empty_components< geode::Surface3D >( 2, "Surface",
                &geode::BRepBuilder::remove_surface, &geode::BRep::surface );
            remove_empty_components< geode::Block3D >( 3, "Volume",
                &geode::BRepBuilder::remove_block, &geode::BRep::block );
        }

        void build_corners()
        {
            for( auto& [tag, elements] : components_[0] )
            {
                const auto first_node = elements.nodes.front();
                if( absl::c_any_of(
                        elements.nodes, [first_node]( geode::index_t node ) {
                            return node != first_node;
                        } ) )
                {
                    geode::Logger::warning( "[MSHInput] Point ", tag,
                        " has several nodes, only the first one is kept" );
                }
                elements.types.resize( 1 );
                elements.nodes.resize( 1 );
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

        /*!
         * Finds the vertex of a component that shares the unique vertex of a
         * vertex of another component.
         */
        std::optional< geode::index_t > component_vertex(
            const geode::ComponentID& from,
            geode::index_t vertex,
            const geode::uuid& to ) const
        {
            const auto unique_vertex = brep_.unique_vertex( { from, vertex } );
            for( const auto& cmv :
                brep_.component_mesh_vertices( unique_vertex ) )
            {
                if( cmv.component_id.id == to )
                {
                    return cmv.vertex;
                }
            }
            return std::nullopt;
        }

        absl::flat_hash_set< geode::uuid > components_around(
            const geode::ComponentID& from,
            geode::index_t vertex,
            const geode::ComponentType& type ) const
        {
            absl::flat_hash_set< geode::uuid > components;
            const auto unique_vertex = brep_.unique_vertex( { from, vertex } );
            for( const auto& cmv :
                brep_.component_mesh_vertices( unique_vertex ) )
            {
                if( cmv.component_id.type == type )
                {
                    components.insert( cmv.component_id.id );
                }
            }
            return components;
        }

        bool is_corner_related( const geode::Corner3D& corner ) const
        {
            return brep_.nb_incidences( corner.id() ) != 0
                   || brep_.nb_embeddings( corner.id() ) != 0;
        }

        /*!
         * Relations between a corner and the lines going through it: boundary
         * at the line ends, internal elsewhere.
         */
        void infer_corner_relations()
        {
            for( const auto& corner : brep_.corners() )
            {
                const auto lines = components_around( corner.component_id(), 0,
                    geode::Line3D::component_type_static() );
                for( const auto& line_id : lines )
                {
                    const auto& line = brep_.line( line_id );
                    if( brep_.is_boundary( corner, line )
                        || brep_.is_internal( corner, line ) )
                    {
                        continue;
                    }
                    const auto vertex =
                        component_vertex( corner.component_id(), 0, line_id );
                    const auto& mesh = line.mesh();
                    if( mesh.edges_around_vertex( vertex.value() ).size() == 2
                        && !is_closed( mesh ) )
                    {
                        builder_.add_corner_line_internal_relationship(
                            corner, line );
                    }
                    else
                    {
                        builder_.add_corner_line_boundary_relationship(
                            corner, line );
                    }
                }
            }
            for( const auto& corner : brep_.corners() )
            {
                if( is_corner_related( corner ) )
                {
                    continue;
                }
                for( const auto& surface_id :
                    components_around( corner.component_id(), 0,
                        geode::Surface3D::component_type_static() ) )
                {
                    builder_.add_corner_surface_internal_relationship(
                        corner, brep_.surface( surface_id ) );
                }
                if( is_corner_related( corner ) )
                {
                    continue;
                }
                for( const auto& block_id :
                    components_around( corner.component_id(), 0,
                        geode::Block3D::component_type_static() ) )
                {
                    builder_.add_corner_block_internal_relationship(
                        corner, brep_.block( block_id ) );
                }
            }
        }

        static bool is_closed( const geode::EdgedCurve3D& mesh )
        {
            for( const auto v : geode::Range{ mesh.nb_vertices() } )
            {
                if( mesh.edges_around_vertex( v ).size() == 1 )
                {
                    return false;
                }
            }
            return true;
        }

        /*!
         * @return the number of polygons around the first line edge in the
         * surface, or nullopt if the line is not in the surface.
         */
        std::optional< geode::index_t > nb_polygons_around_line(
            const geode::Line3D& line, const geode::Surface3D& surface ) const
        {
            if( !contains_all_vertices( line, surface.id() ) )
            {
                return std::nullopt;
            }
            const auto& line_mesh = line.mesh();
            std::array< geode::index_t, 2 > edge_vertices;
            for( const auto v : geode::LRange{ 2 } )
            {
                edge_vertices[v] = component_vertex( line.component_id(),
                    line_mesh.edge_vertex( { 0, v } ), surface.id() )
                                       .value();
            }
            const auto nb_polygons =
                surface.mesh()
                    .polygons_from_edge_vertices( edge_vertices )
                    .size();
            if( nb_polygons == 0 )
            {
                return std::nullopt;
            }
            return nb_polygons;
        }

        /*!
         * Relations between a line and the surfaces containing all its edges:
         * internal if the line is inside the surface, boundary otherwise.
         * Lines inside no surface may be embedded in blocks.
         */
        void infer_line_relations()
        {
            for( const auto& line : brep_.lines() )
            {
                if( line.mesh().nb_edges() == 0 )
                {
                    continue;
                }
                const auto first_vertex = line.mesh().edge_vertex( { 0, 0 } );
                for( const auto& surface_id :
                    components_around( line.component_id(), first_vertex,
                        geode::Surface3D::component_type_static() ) )
                {
                    const auto& surface = brep_.surface( surface_id );
                    if( brep_.is_boundary( line, surface )
                        || brep_.is_internal( line, surface ) )
                    {
                        continue;
                    }
                    const auto nb_polygons =
                        nb_polygons_around_line( line, surface );
                    if( !nb_polygons )
                    {
                        continue;
                    }
                    if( nb_polygons.value() == 2 )
                    {
                        builder_.add_line_surface_internal_relationship(
                            line, surface );
                    }
                    else
                    {
                        builder_.add_line_surface_boundary_relationship(
                            line, surface );
                    }
                }
                if( brep_.nb_incidences( line.id() ) != 0
                    || brep_.nb_embeddings( line.id() ) != 0 )
                {
                    continue;
                }
                for( const auto& block_id :
                    components_around( line.component_id(), first_vertex,
                        geode::Block3D::component_type_static() ) )
                {
                    if( contains_all_vertices( line, block_id ) )
                    {
                        builder_.add_line_block_internal_relationship(
                            line, brep_.block( block_id ) );
                    }
                }
            }
        }

        template < typename Component >
        bool contains_all_vertices(
            const Component& component, const geode::uuid& other ) const
        {
            for( const auto v : geode::Range{ component.mesh().nb_vertices() } )
            {
                if( !component_vertex( component.component_id(), v, other ) )
                {
                    return false;
                }
            }
            return true;
        }

        /*!
         * @return the number of polyhedra around the first surface polygon in
         * the block, or nullopt if the surface is not in the block.
         */
        std::optional< geode::index_t > nb_polyhedra_around_surface(
            const geode::Surface3D& surface, const geode::Block3D& block ) const
        {
            if( !contains_all_vertices( surface, block.id() ) )
            {
                return std::nullopt;
            }
            const auto& surface_mesh = surface.mesh();
            geode::PolyhedronFacetVertices facet_vertices;
            for( const auto v :
                geode::LRange{ surface_mesh.nb_polygon_vertices( 0 ) } )
            {
                facet_vertices.push_back(
                    component_vertex( surface.component_id(),
                        surface_mesh.polygon_vertex( { 0, v } ), block.id() )
                        .value() );
            }
            const auto nb_polyhedra = block.mesh()
                                          .polyhedra_from_facet_vertices(
                                              std::move( facet_vertices ) )
                                          .size();
            if( nb_polyhedra == 0 )
            {
                return std::nullopt;
            }
            return nb_polyhedra;
        }

        /*!
         * Relations between a surface and the blocks containing all its
         * polygons: internal if the surface is inside the block, boundary
         * otherwise.
         */
        void infer_surface_relations()
        {
            for( const auto& surface : brep_.surfaces() )
            {
                if( surface.mesh().nb_polygons() == 0 )
                {
                    continue;
                }
                const auto first_vertex =
                    surface.mesh().polygon_vertex( { 0, 0 } );
                for( const auto& block_id :
                    components_around( surface.component_id(), first_vertex,
                        geode::Block3D::component_type_static() ) )
                {
                    const auto& block = brep_.block( block_id );
                    if( brep_.is_boundary( surface, block )
                        || brep_.is_internal( surface, block ) )
                    {
                        continue;
                    }
                    const auto nb_polyhedra =
                        nb_polyhedra_around_surface( surface, block );
                    if( !nb_polyhedra )
                    {
                        continue;
                    }
                    if( nb_polyhedra.value() == 2 )
                    {
                        builder_.add_surface_block_internal_relationship(
                            surface, block );
                    }
                    else
                    {
                        builder_.add_surface_block_boundary_relationship(
                            surface, block );
                    }
                }
            }
        }

        void unset_internal_line_adjacencies()
        {
            for( const auto& surface : brep_.surfaces() )
            {
                std::vector< geode::PolygonEdge > polygon_edges;
                for( const auto& line : brep_.internal_lines( surface ) )
                {
                    const auto& line_mesh = line.mesh();
                    for( const auto e : geode::Range{ line_mesh.nb_edges() } )
                    {
                        const auto v0 = component_vertex( line.component_id(),
                            line_mesh.edge_vertex( { e, 0 } ), surface.id() );
                        const auto v1 = component_vertex( line.component_id(),
                            line_mesh.edge_vertex( { e, 1 } ), surface.id() );
                        if( !v0 || !v1 )
                        {
                            continue;
                        }
                        for( const auto& edge :
                            surface.mesh().polygons_from_edge_vertices(
                                { v0.value(), v1.value() } ) )
                        {
                            polygon_edges.push_back( edge );
                        }
                    }
                }
                if( polygon_edges.empty() )
                {
                    continue;
                }
                auto mesh_builder = builder_.surface_mesh_builder( surface );
                for( const auto& edge : polygon_edges )
                {
                    mesh_builder->unset_polygon_adjacent( edge );
                }
            }
        }

        void unset_internal_surface_adjacencies()
        {
            for( const auto& block : brep_.blocks() )
            {
                std::vector< geode::PolyhedronFacet > polyhedron_facets;
                for( const auto& surface : brep_.internal_surfaces( block ) )
                {
                    const auto& surface_mesh = surface.mesh();
                    for( const auto p :
                        geode::Range{ surface_mesh.nb_polygons() } )
                    {
                        geode::PolyhedronFacetVertices facet_vertices;
                        for( const auto v : geode::LRange{
                                 surface_mesh.nb_polygon_vertices( p ) } )
                        {
                            const auto vertex =
                                component_vertex( surface.component_id(),
                                    surface_mesh.polygon_vertex( { p, v } ),
                                    block.id() );
                            if( !vertex )
                            {
                                break;
                            }
                            facet_vertices.push_back( vertex.value() );
                        }
                        if( facet_vertices.size()
                            != surface_mesh.nb_polygon_vertices( p ) )
                        {
                            continue;
                        }
                        for( const auto& facet :
                            block.mesh().polyhedra_from_facet_vertices(
                                std::move( facet_vertices ) ) )
                        {
                            polyhedron_facets.push_back( facet );
                        }
                    }
                }
                if( polyhedron_facets.empty() )
                {
                    continue;
                }
                auto mesh_builder = builder_.block_mesh_builder( block );
                for( const auto& facet : polyhedron_facets )
                {
                    mesh_builder->unset_polyhedron_adjacent( facet );
                }
            }
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
