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

#include <geode/io/model/private/svg_input.h>

#include <cctype>
#include <fstream>

#include <pugixml.hpp>

#include <geode/geometry/bounding_box.h>
#include <geode/geometry/nn_search.h>
#include <geode/geometry/point.h>
#include <geode/geometry/vector.h>

#include <geode/mesh/builder/edged_curve_builder.h>
#include <geode/mesh/builder/point_set_builder.h>
#include <geode/mesh/core/edged_curve.h>

#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/representation/builder/section_builder.h>
#include <geode/model/representation/core/section.h>

namespace
{
    constexpr double FRACTION = 1e-5;

    class SVGInputImpl
    {
    public:
        SVGInputImpl( absl::string_view filename, geode::Section& section )
            : file_{ geode::to_string( filename ) },
              section_( section ),
              builder_{ section }
        {
            OPENGEODE_EXCEPTION( file_.good(),
                "[SVGInput] Error while opening file: ", filename );
            const auto ok =
                document_.load_file( geode::to_string( filename ).c_str() );
            OPENGEODE_EXCEPTION(
                ok, "[SVGInput] Error while parsing file: ", filename );
        }

        void read_file()
        {
            for( const auto& group : document_.child( "svg" ).children( "g" ) )
            {
                read_group_paths( group );
            }
        }

        void process_paths()
        {
            for( auto& path : paths_ )
            {
                format_path( path );
                auto tokens = split_path( path );
                process_tokens( tokens );
            }
        }

        void build_topology()
        {
            const auto epsilon = compute_epsilon();
            const geode::NNSearch2D colocater( potential_corners_ );
            const auto colocated_info =
                colocater.colocated_index_mapping( epsilon );
            const auto corner_ids = create_corners( colocated_info );
            build_corner_line_relations( colocated_info, corner_ids );
            create_line_unique_vertices();
        }

    private:
        struct Command
        {
            Command() = default;

            void update( const char token )
            {
                letter = std::tolower( token );
                absolute = std::isupper( token );
            }

            geode::Point2D apply( const geode::Point2D& position,
                const std::vector< double >& params ) const
            {
                OPENGEODE_ASSERT( params.size() == get_nb_params(),
                    "[SVGInput::Command::apply] Wrong number of parameters" );
                if( letter == 'm' || letter == 'l' )
                {
                    geode::Point2D step{ { params[0], params[1] } };
                    if( absolute )
                    {
                        return step;
                    }
                    return position + step;
                }
                if( letter == 'h' )
                {
                    if( absolute )
                    {
                        return geode::Point2D{ { params[0],
                            position.value( 1 ) } };
                    }
                    return geode::Point2D{ { position.value( 0 ) + params[0],
                        position.value( 1 ) } };
                }
                else if( letter == 'v' )
                {
                    if( absolute )
                    {
                        return geode::Point2D{ {
                            position.value( 0 ),
                            params[0],
                        } };
                    }
                    return geode::Point2D{ { position.value( 0 ),
                        position.value( 1 ) + params[0] } };
                }
                throw geode::OpenGeodeException(
                    "[SVGInput::Command::apply] Command not supported: "
                    + letter );
                return position;
            }

            geode::index_t get_nb_params() const
            {
                return param_map.at( letter );
            }

            const absl::flat_hash_map< char, geode::index_t > param_map{
                { 'm', 2 }, { 'l', 2 }, { 'h', 1 }, { 'v', 1 }, { 'z', 0 }
            };

            char letter{ 'l' };
            bool absolute{ true };
        };

        void read_group_paths( const pugi::xml_node& group )
        {
            for( const auto& path : group.children( "path" ) )
            {
                paths_.push_back( path.attribute( "d" ).value() );
            }
            for( const auto& child_group : group.children( "g" ) )
            {
                read_group_paths( child_group );
            }
        }

        void format_path( std::string& path ) const
        {
            remove_commas( path );
            add_space_around_letters( path );
        }

        std::vector< std::string > split_path( const std::string& path ) const
        {
            std::istringstream iss( path );
            return { std::istream_iterator< std::string >( iss ), {} };
        }

        bool string_isalpha( const std::string& str ) const
        {
            return std::all_of( str.begin(), str.end(), []( const char& c ) {
                return std::isalpha( c );
            } );
        }

        void remove_commas( std::string& path ) const
        {
            std::replace( path.begin(), path.end(), ',', ' ' );
        }

        void add_spaces( std::string& path, char letter ) const
        {
            std::size_t start{ 0 };
            std::size_t found;
            while( ( found = path.find( letter, start ) ) != std::string::npos )
            {
                path.replace( found, 1,
                    absl::StrCat( " ", std::string( 1, letter ), " " ) );
                start = found + 3;
            }
        }

        template < typename... Letters >
        void add_spaces(
            std::string& path, char letter, Letters... others ) const
        {
            add_spaces( path, letter );
            add_spaces( path, others... );
        }

        void add_space_around_letters( std::string& path ) const
        {
            add_spaces(
                path, 'M', 'm', 'L', 'l', 'H', 'h', 'V', 'v', 'Z', 'z' );
        }

        std::vector< double > get_params(
            const std::vector< std::string >& tokens,
            geode::index_t first,
            geode::index_t nb ) const
        {
            std::vector< double > params( nb );
            for( const auto t : geode::Range{ nb } )
            {
                const auto& token = tokens[first + t];
                try
                {
                    params[t] = std::stod( token );
                }
                catch( std::invalid_argument& /*unused*/ )
                {
                    throw geode::OpenGeodeException{
                        "[SVGInputImpl::get_params] Path token is not a "
                        "number: "
                        + token
                    };
                }
            }
            return params;
        }

        geode::index_t update_command( const std::vector< std::string >& tokens,
            geode::index_t t,
            Command& command ) const
        {
            const auto& token = tokens[t];
            if( string_isalpha( token ) )
            {
                OPENGEODE_EXCEPTION( token.size() == 1,
                    "[SVGInputImpl::update_command] Command "
                    "should be single letter" );
                command.update( *token.c_str() );
                return t + 1;
            }

            command.letter = 'l';
            return t;
        }

        geode::index_t apply_command( const std::vector< std::string >& tokens,
            geode::index_t t,
            Command& command,
            geode::Point2D& cur_position,
            std::vector< geode::Point2D >& vertices )
        {
            const auto nb_params = command.get_nb_params();
            const auto params = get_params( tokens, t, nb_params );
            if( command.letter == 'z' )
            {
                vertices.push_back( vertices.front() );
                create_line( vertices );
                vertices.clear();
                return t;
            }
            if( command.letter == 'm' && !vertices.empty() )
            {
                if( !vertices.empty() )
                {
                    create_line( vertices );
                    vertices.clear();
                }
            }
            cur_position = command.apply( cur_position, params );
            vertices.push_back( cur_position );
            return t + nb_params - 1;
        }

        void process_tokens( const std::vector< std::string >& tokens )
        {
            std::vector< geode::Point2D > vertices;
            Command cur_command;
            geode::Point2D cur_position;
            geode::index_t t{ 0 };
            while( t < tokens.size() )
            {
                t = update_command( tokens, t, cur_command );
                t = apply_command(
                    tokens, t, cur_command, cur_position, vertices );
                t++;
            }
            create_line( vertices );
        }

        void add_potential_corner( const geode::Point2D& point,
            const geode::uuid& line_id,
            const geode::index_t line_vertex )
        {
            potential_corners_.push_back( point );
            potential_corner_cmv_.emplace_back(
                geode::ComponentID{
                    geode::Line2D::component_type_static(), line_id },
                line_vertex );
        }

        bool boundary_relation_exist(
            const geode::Corner2D& corner, const geode::Line2D& line ) const
        {
            for( const auto& boundary : section_.boundaries( line ) )
            {
                if( boundary.id() == corner.id() )
                {
                    return true;
                }
            }
            return false;
        }

        double compute_epsilon() const
        {
            const auto bbox = section_.bounding_box();
            return FRACTION
                   * geode::Vector2D{ bbox.min(), bbox.max() }.length();
        }

        void create_line( const std::vector< geode::Point2D >& vertices )
        {
            if( vertices.size() < 2 )
            {
                return;
            }
            const auto& line_id = builder_.add_line();
            const auto line_builder = builder_.line_mesh_builder( line_id );
            line_builder->create_point( vertices.front() );
            for( const auto v : geode::Range{ 1, vertices.size() } )
            {
                line_builder->create_point( vertices[v] );
                line_builder->create_edge( v - 1, v );
            }
            add_potential_corner( vertices.front(), line_id, 0 );
            add_potential_corner(
                vertices.back(), line_id, vertices.size() - 1 );
        }

        std::vector< geode::uuid > create_corners(
            const geode::NNSearch2D::ColocatedInfo& colocated_info )
        {
            std::vector< geode::uuid > corner_ids;
            corner_ids.reserve( colocated_info.nb_unique_points() );
            for( const auto& unique_point : colocated_info.unique_points )
            {
                const auto corner_id = builder_.add_corner();
                builder_.corner_mesh_builder( corner_id )
                    ->create_point( unique_point );
                const auto uv = builder_.create_unique_vertex();
                builder_.set_unique_vertex(
                    { section_.corner( corner_id ).component_id(), 0 }, uv );
                corner_ids.push_back( corner_id );
            }
            return corner_ids;
        }

        void build_corner_line_relations(
            const geode::NNSearch2D::ColocatedInfo& colocated_info,
            const std::vector< geode::uuid >& corner_ids )
        {
            for( const auto& c : geode::Range{ potential_corner_cmv_.size() } )
            {
                const auto mapped_corner = colocated_info.colocated_mapping[c];
                const auto& corner =
                    section_.corner( corner_ids[mapped_corner] );
                const auto& line =
                    section_.line( potential_corner_cmv_[c].component_id.id() );
                if( !boundary_relation_exist( corner, line ) )
                {
                    builder_.add_corner_line_boundary_relationship(
                        corner, line );
                }
                builder_.set_unique_vertex(
                    potential_corner_cmv_[c], mapped_corner );
            }
        }

        void create_line_unique_vertices()
        {
            for( const auto& line : section_.lines() )
            {
                const auto& mesh = line.mesh();
                for( const auto v : geode::Range{ mesh.nb_vertices() } )
                {
                    const auto cmv =
                        geode::ComponentMeshVertex{ line.component_id(), v };
                    const auto unique_vertex = section_.unique_vertex( cmv );
                    if( unique_vertex == geode::NO_ID )
                    {
                        builder_.set_unique_vertex(
                            cmv, builder_.create_unique_vertex() );
                    }
                }
            }
        }

    private:
        std::ifstream file_;
        geode::Section& section_;
        geode::SectionBuilder builder_;
        pugi::xml_document document_;
        std::vector< std::string > paths_;
        std::vector< geode::Point2D > potential_corners_;
        std::vector< geode::ComponentMeshVertex > potential_corner_cmv_;
    };
} // namespace

namespace geode
{
    namespace detail
    {
        Section SVGInput::read()
        {
            Section section;
            SVGInputImpl impl( filename(), section );
            impl.read_file();
            impl.process_paths();
            impl.build_topology();
            return section;
        }
    } // namespace detail
} // namespace geode
