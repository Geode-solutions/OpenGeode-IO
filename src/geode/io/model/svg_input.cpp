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

#include <geode/io/model/internal/svg_input.hpp>

#include <array>
#include <cmath>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <absl/algorithm/container.h>
#include <absl/strings/ascii.h>
#include <absl/strings/charconv.h>
#include <absl/strings/match.h>
#include <absl/types/span.h>

#include <pugixml.hpp>

#include <geode/geometry/bounding_box.hpp>
#include <geode/geometry/nn_search.hpp>
#include <geode/geometry/point.hpp>
#include <geode/geometry/vector.hpp>

#include <geode/mesh/builder/edged_curve_builder.hpp>
#include <geode/mesh/builder/point_set_builder.hpp>
#include <geode/mesh/core/edged_curve.hpp>

#include <geode/model/mixin/core/corner.hpp>
#include <geode/model/mixin/core/line.hpp>
#include <geode/model/representation/builder/section_builder.hpp>
#include <geode/model/representation/core/section.hpp>

namespace
{
    constexpr double FRACTION = 1e-5;

    /// 2D affine transform (a c e / b d f / 0 0 1), as in SVG matrix()
    struct Affine2D
    {
        Affine2D operator*( const Affine2D& other ) const
        {
            return { a * other.a + c * other.b, b * other.a + d * other.b,
                a * other.c + c * other.d, b * other.c + d * other.d,
                a * other.e + c * other.f + e, b * other.e + d * other.f + f };
        }

        geode::Point2D apply( const geode::Point2D& point ) const
        {
            const auto x = point.value( 0 );
            const auto y = point.value( 1 );
            return geode::Point2D{ { a * x + c * y + e, b * x + d * y + f } };
        }

        double a{ 1 };
        double b{ 0 };
        double c{ 0 };
        double d{ 1 };
        double e{ 0 };
        double f{ 0 };
    };

    struct SVGPath
    {
        std::string data;
        Affine2D transform;
    };

    class SVGInputImpl
    {
    public:
        SVGInputImpl( std::string_view filename, geode::Section& section )
            : file_{ geode::to_string( filename ) },
              section_( section ),
              builder_{ section }
        {
            geode::OpenGeodeIOModelException::check_exception( file_.good(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[SVGInput] Error while opening file: ", filename );
            const auto loaded =
                document_.load_file( geode::to_string( filename ).c_str() );
            geode::OpenGeodeIOModelException::check_exception( loaded, nullptr,
                geode::OpenGeodeException::TYPE::internal,
                "[SVGInput] Error while parsing file: ", filename );
        }

        void read_file()
        {
            const auto root = document_.child( "svg" );
            geode::OpenGeodeIOModelException::check_exception( !root.empty(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[SVGInput] No <svg> root element" );
            read_container_paths( root, read_transform( root ) );
        }

        void process_paths()
        {
            for( const auto& path : paths_ )
            {
                transform_ = path.transform;
                process_tokens( split_path( path.data ) );
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
        /*!
         * Reads the paths rendered from a container element (svg, g, a).
         * Other elements, such as defs or symbol, are not rendered by
         * themselves and are ignored.
         */
        void read_container_paths(
            const pugi::xml_node& container, const Affine2D& transform )
        {
            for( const auto& child : container.children() )
            {
                const std::string_view name{ child.name() };
                if( name == "path" )
                {
                    paths_.push_back( { child.attribute( "d" ).value(),
                        transform * read_transform( child ) } );
                }
                else if( name == "g" || name == "a" || name == "svg" )
                {
                    read_container_paths(
                        child, transform * read_transform( child ) );
                }
            }
        }

        static bool is_command_letter( char character )
        {
            switch( character )
            {
                case 'M':
                case 'm':
                case 'L':
                case 'l':
                case 'H':
                case 'h':
                case 'V':
                case 'v':
                case 'Z':
                case 'z':
                case 'C':
                case 'c':
                case 'S':
                case 's':
                case 'Q':
                case 'q':
                case 'T':
                case 't':
                case 'A':
                case 'a':
                    return true;
                default:
                    return false;
            }
        }

        static bool is_separator( char character )
        {
            return character == ',' || character == ' ' || character == '\t'
                   || character == '\n' || character == '\r'
                   || character == '\f';
        }

        /*!
         * Splits the path data in a single pass: each command letter is a
         * token, numbers are separated by whitespaces, commas, or by the
         * start of the next number in compact data ("10-5", ".5.5").
         */
        static std::vector< std::string_view > split_path(
            std::string_view path )
        {
            std::vector< std::string_view > tokens;
            std::size_t start{ 0 };
            bool in_token{ false };
            bool has_dot{ false };
            bool has_exponent{ false };
            const auto flush = [&]( std::size_t end ) {
                if( in_token )
                {
                    tokens.push_back( path.substr( start, end - start ) );
                    in_token = false;
                }
            };
            for( const auto c : geode::Range{ path.size() } )
            {
                const auto character = path[c];
                if( is_separator( character ) )
                {
                    flush( c );
                    continue;
                }
                if( is_command_letter( character ) )
                {
                    flush( c );
                    tokens.push_back( path.substr( c, 1 ) );
                    continue;
                }
                if( in_token )
                {
                    const auto previous = path[c - 1];
                    const auto is_sign = character == '-' || character == '+';
                    const auto after_exponent =
                        previous == 'e' || previous == 'E';
                    const auto new_number =
                        ( is_sign && !after_exponent )
                        || ( character == '.' && ( has_dot || has_exponent ) );
                    if( !new_number )
                    {
                        has_dot = has_dot || character == '.';
                        has_exponent = has_exponent || character == 'e'
                                       || character == 'E';
                        continue;
                    }
                    flush( c );
                }
                start = c;
                in_token = true;
                has_dot = character == '.';
                has_exponent = false;
            }
            flush( path.size() );
            return tokens;
        }

        static Affine2D transform_function(
            std::string_view name, absl::Span< const double > args )
        {
            const auto check_nb_args = [&]( std::size_t min, std::size_t max ) {
                geode::OpenGeodeIOModelException::check_exception(
                    args.size() >= min && args.size() <= max, nullptr,
                    geode::OpenGeodeException::TYPE::data,
                    "[SVGInput] Wrong number of parameters for transform ",
                    name );
            };
            static constexpr double DEGREES_TO_RADIANS{ 3.14159265358979323846
                                                        / 180. };
            if( name == "matrix" )
            {
                check_nb_args( 6, 6 );
                return { args[0], args[1], args[2], args[3], args[4], args[5] };
            }
            if( name == "translate" )
            {
                check_nb_args( 1, 2 );
                return { 1, 0, 0, 1, args[0], args.size() == 2 ? args[1] : 0. };
            }
            if( name == "scale" )
            {
                check_nb_args( 1, 2 );
                return { args[0], 0, 0, args.size() == 2 ? args[1] : args[0], 0,
                    0 };
            }
            if( name == "rotate" )
            {
                check_nb_args( 1, 3 );
                geode::OpenGeodeIOModelException::check_exception(
                    args.size() != 2, nullptr,
                    geode::OpenGeodeException::TYPE::data,
                    "[SVGInput] rotate() takes 1 or 3 parameters" );
                const auto angle = args[0] * DEGREES_TO_RADIANS;
                const Affine2D rotation{ std::cos( angle ), std::sin( angle ),
                    -std::sin( angle ), std::cos( angle ), 0, 0 };
                if( args.size() == 1 )
                {
                    return rotation;
                }
                return Affine2D{ 1, 0, 0, 1, args[1], args[2] } * rotation
                       * Affine2D{ 1, 0, 0, 1, -args[1], -args[2] };
            }
            if( name == "skewX" )
            {
                check_nb_args( 1, 1 );
                return { 1, 0, std::tan( args[0] * DEGREES_TO_RADIANS ), 1, 0,
                    0 };
            }
            if( name == "skewY" )
            {
                check_nb_args( 1, 1 );
                return { 1, std::tan( args[0] * DEGREES_TO_RADIANS ), 0, 1, 0,
                    0 };
            }
            throw geode::OpenGeodeIOModelException{ nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[SVGInput] Unknown transform function: ", name };
        }

        /*!
         * Reads the transform attribute of an element, such as
         * "translate(10, 20) rotate(45)".
         */
        static Affine2D read_transform( const pugi::xml_node& node )
        {
            std::string_view transform{ node.attribute( "transform" ).value() };
            Affine2D result;
            while( true )
            {
                while( !transform.empty()
                       && ( is_separator( transform.front() ) ) )
                {
                    transform.remove_prefix( 1 );
                }
                if( transform.empty() )
                {
                    return result;
                }
                const auto open = transform.find( '(' );
                const auto close = transform.find( ')' );
                geode::OpenGeodeIOModelException::check_exception(
                    open != std::string_view::npos
                        && close != std::string_view::npos && open < close,
                    nullptr, geode::OpenGeodeException::TYPE::data,
                    "[SVGInput] Invalid transform attribute: ",
                    node.attribute( "transform" ).value() );
                const auto name =
                    absl::StripAsciiWhitespace( transform.substr( 0, open ) );
                std::vector< double > args;
                for( const auto& token : split_path(
                         transform.substr( open + 1, close - open - 1 ) ) )
                {
                    args.push_back( read_number( token ) );
                }
                result = result * transform_function( name, args );
                transform.remove_prefix( close + 1 );
            }
        }

        struct PathState
        {
            char command{ 0 };
            bool absolute{ true };
            geode::Point2D current;
            geode::Point2D subpath_start;
            std::vector< geode::Point2D > vertices;
        };

        static geode::index_t nb_parameters( char command )
        {
            switch( command )
            {
                case 'm':
                case 'l':
                    return 2;
                case 'h':
                case 'v':
                    return 1;
                case 'z':
                    return 0;
                default:
                    throw geode::OpenGeodeIOModelException{ nullptr,
                        geode::OpenGeodeException::TYPE::data,
                        "[SVGInput] Unsupported path command '",
                        std::string( 1, command ),
                        "': only M, L, H, V and Z (and lower case) are "
                        "supported" };
            }
        }

        static double read_number( std::string_view token )
        {
            double value{ 0 };
            // from_chars does not accept an explicit plus sign
            const auto number =
                absl::StartsWith( token, "+" ) ? token.substr( 1 ) : token;
            const auto result = absl::from_chars(
                number.data(), number.data() + number.size(), value );
            // SVG numbers are decimal: no hexadecimal, nan or inf
            const auto is_svg_number_character = []( char character ) {
                return absl::ascii_isdigit(
                           static_cast< unsigned char >( character ) )
                       || character == '.' || character == 'e'
                       || character == 'E' || character == '-'
                       || character == '+';
            };
            geode::OpenGeodeIOModelException::check_exception(
                absl::c_all_of( number, is_svg_number_character )
                    && result.ec == std::errc{} && !number.empty()
                    && result.ptr == number.data() + number.size()
                    && std::isfinite( value ),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[SVGInput] Path token is not a number: \"", token, "\"" );
            return value;
        }

        void close_subpath( PathState& state )
        {
            if( state.vertices.size() > 1 )
            {
                if( state.vertices.back() != state.subpath_start )
                {
                    state.vertices.push_back( state.subpath_start );
                }
                create_line( state.vertices );
            }
            state.vertices.clear();
            state.current = state.subpath_start;
        }

        /*!
         * Applies one parameter group of the current command.
         * @return the index of the token after the parameters
         */
        geode::index_t apply_command(
            absl::Span< const std::string_view > tokens,
            geode::index_t token_id,
            PathState& state )
        {
            const auto nb_params = nb_parameters( state.command );
            geode::OpenGeodeIOModelException::check_exception(
                token_id + nb_params <= tokens.size(), nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[SVGInput] Path command '", std::string( 1, state.command ),
                "' misses parameters" );
            std::array< double, 2 > params{ 0, 0 };
            for( const auto p : geode::Range{ nb_params } )
            {
                params[p] = read_number( tokens[token_id + p] );
            }
            const auto x = state.current.value( 0 );
            const auto y = state.current.value( 1 );
            geode::Point2D next;
            if( state.command == 'm' || state.command == 'l' )
            {
                next = state.absolute
                           ? geode::Point2D{ { params[0], params[1] } }
                           : geode::Point2D{ { x + params[0], y + params[1] } };
            }
            else if( state.command == 'h' )
            {
                next = geode::Point2D{
                    { state.absolute ? params[0] : x + params[0], y }
                };
            }
            else
            {
                next = geode::Point2D{ { x,
                    state.absolute ? params[0] : y + params[0] } };
            }
            if( state.command == 'm' )
            {
                create_line( state.vertices );
                state.vertices.clear();
                state.subpath_start = next;
                // Parameter groups after a moveto are implicit linetos
                state.command = 'l';
            }
            else if( state.vertices.empty() )
            {
                // Drawing right after a closepath restarts at its start
                state.vertices.push_back( state.current );
            }
            state.vertices.push_back( next );
            state.current = next;
            return token_id + nb_params;
        }

        void process_tokens( absl::Span< const std::string_view > tokens )
        {
            PathState state;
            geode::index_t token_id{ 0 };
            while( token_id < tokens.size() )
            {
                const auto& token = tokens[token_id];
                if( token.size() == 1 && is_command_letter( token.front() ) )
                {
                    state.command = static_cast< char >( absl::ascii_tolower(
                        static_cast< unsigned char >( token.front() ) ) );
                    state.absolute = absl::ascii_isupper(
                        static_cast< unsigned char >( token.front() ) );
                    token_id++;
                    if( state.command == 'z' )
                    {
                        close_subpath( state );
                        continue;
                    }
                }
                else
                {
                    geode::OpenGeodeIOModelException::check_exception(
                        state.command != 0 && state.command != 'z', nullptr,
                        geode::OpenGeodeException::TYPE::data,
                        "[SVGInput] Path number \"", token,
                        "\" is not preceded by a command" );
                }
                token_id = apply_command( tokens, token_id, state );
            }
            create_line( state.vertices );
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
            const auto line_builder =
                builder_.line_mesh_builder( section_.line( line_id ) );
            const auto first = transform_.apply( vertices.front() );
            line_builder->create_point( first );
            for( const auto v : geode::Range{ 1, vertices.size() } )
            {
                line_builder->create_point( transform_.apply( vertices[v] ) );
                line_builder->create_edge( v - 1, v );
            }
            add_potential_corner( first, line_id, 0 );
            add_potential_corner( transform_.apply( vertices.back() ), line_id,
                vertices.size() - 1 );
        }

        std::vector< geode::uuid > create_corners(
            const geode::NNSearch2D::ColocatedInfo& colocated_info )
        {
            std::vector< geode::uuid > corner_ids;
            corner_ids.reserve( colocated_info.nb_unique_points() );
            for( const auto& unique_point : colocated_info.unique_points )
            {
                const auto corner_id = builder_.add_corner();
                builder_.corner_mesh_builder( section_.corner( corner_id ) )
                    ->create_point( unique_point );
                const auto uv_id = builder_.create_unique_vertex();
                builder_.set_unique_vertex(
                    { section_.corner( corner_id ).component_id(), 0 }, uv_id );
                corner_ids.push_back( corner_id );
            }
            return corner_ids;
        }

        void build_corner_line_relations(
            const geode::NNSearch2D::ColocatedInfo& colocated_info,
            const std::vector< geode::uuid >& corner_ids )
        {
            for( const auto& cmv :
                geode::Range{ potential_corner_cmv_.size() } )
            {
                const auto mapped_corner =
                    colocated_info.colocated_mapping[cmv];
                const auto& corner =
                    section_.corner( corner_ids[mapped_corner] );
                const auto& line =
                    section_.line( potential_corner_cmv_[cmv].component_id.id );
                if( !boundary_relation_exist( corner, line ) )
                {
                    builder_.add_corner_line_boundary_relationship(
                        corner, line );
                }
                builder_.set_unique_vertex(
                    potential_corner_cmv_[cmv], mapped_corner );
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
        std::vector< SVGPath > paths_;
        Affine2D transform_;
        std::vector< geode::Point2D > potential_corners_;
        std::vector< geode::ComponentMeshVertex > potential_corner_cmv_;
    };
} // namespace

namespace geode::internal
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

    Percentage SVGInput::is_loadable() const
    {
        Section section;
        SVGInputImpl impl( filename(), section );
        return Percentage{ 1 };
    }
} // namespace geode::internal