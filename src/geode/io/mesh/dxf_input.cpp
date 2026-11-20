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

#include <geode/io/mesh/internal/dxf_input.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/match.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>
#include <absl/strings/str_join.h>
#include <absl/strings/strip.h>

#include <geode/basic/logger.hpp>
#include <geode/basic/string.hpp>

#include <geode/geometry/nn_search.hpp>
#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/polygonal_surface_builder.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>

#include <geode/io/mesh/common.hpp>

namespace
{
    constexpr std::string_view BINARY_SENTINEL{ "AutoCAD Binary DXF" };
    constexpr std::string_view UTF8_BOM{ "\xEF\xBB\xBF" };
    constexpr int POLYFACE_MESH_FLAG{ 64 };
    constexpr int POLYFACE_VERTEX_FLAG{ 64 };
    constexpr int FACE_RECORD_FLAG{ 128 };
    constexpr int FIRST_FACE_INDEX_CODE{ 71 };
    constexpr int NB_FACE_INDICES{ 4 };
    constexpr int NB_3DFACE_CORNERS{ 4 };
    constexpr int XDATA_APPLICATION_CODE{ 1001 };
    constexpr int XDATA_STRING_CODE{ 1000 };
    constexpr int XDATA_REAL_CODE{ 1040 };
    constexpr int XDATA_INTEGER_CODE{ 1070 };
    constexpr std::array< std::string_view, 2 > NAME_APPLICATIONS{ "Name",
        "Nom de Galerie" };

    struct GroupPair
    {
        int code{ 0 };
        std::string value;
    };

    struct Entity
    {
        std::string type;
        std::vector< GroupPair > groups;
    };

    struct SurfaceData
    {
        std::string name;
        std::vector< geode::Point3D > points;
        std::vector< std::vector< geode::index_t > > polygons;
        bool colocate{ false };
    };

    struct SkippedFaces
    {
        geode::index_t degenerate{ 0 };
        geode::index_t out_of_range{ 0 };
        geode::index_t repeated_vertex{ 0 };

        [[nodiscard]] geode::index_t total() const
        {
            return degenerate + out_of_range + repeated_vertex;
        }
    };

    enum struct PolygonStatus
    {
        valid,
        degenerate,
        repeated_vertex
    };

    /*!
     * Removes consecutive duplicates, including the wrap-around
     * (a,b,c,a -> a,b,c), and tells if the polygon is usable.
     */
    template < typename Value >
    PolygonStatus clean_polygon( std::vector< Value >& vertices )
    {
        std::vector< Value > cleaned;
        cleaned.reserve( vertices.size() );
        for( const auto& vertex : vertices )
        {
            if( cleaned.empty() || !( cleaned.back() == vertex ) )
            {
                cleaned.push_back( vertex );
            }
        }
        while( cleaned.size() > 1 && cleaned.back() == cleaned.front() )
        {
            cleaned.pop_back();
        }
        vertices = std::move( cleaned );
        if( vertices.size() < 3 )
        {
            return PolygonStatus::degenerate;
        }
        for( const auto i : geode::Indices{ vertices } )
        {
            for( const auto j : geode::Range{ i + 1, vertices.size() } )
            {
                if( vertices[i] == vertices[j] )
                {
                    return PolygonStatus::repeated_vertex;
                }
            }
        }
        return PolygonStatus::valid;
    }

    void count_skipped( PolygonStatus status, SkippedFaces& skipped )
    {
        if( status == PolygonStatus::degenerate )
        {
            skipped.degenerate++;
        }
        else if( status == PolygonStatus::repeated_vertex )
        {
            skipped.repeated_vertex++;
        }
    }

    void append_utf8( std::string& output, unsigned int code_point )
    {
        if( code_point < 0x80 )
        {
            output.push_back( static_cast< char >( code_point ) );
        }
        else if( code_point < 0x800 )
        {
            output.push_back(
                static_cast< char >( 0xC0 | ( code_point >> 6 ) ) );
            output.push_back(
                static_cast< char >( 0x80 | ( code_point & 0x3F ) ) );
        }
        else
        {
            output.push_back(
                static_cast< char >( 0xE0 | ( code_point >> 12 ) ) );
            output.push_back(
                static_cast< char >( 0x80 | ( ( code_point >> 6 ) & 0x3F ) ) );
            output.push_back(
                static_cast< char >( 0x80 | ( code_point & 0x3F ) ) );
        }
    }

    /*!
     * Decodes the \U+XXXX escapes of DXF strings to UTF-8.
     */
    std::string decode_unicode_escapes( std::string_view value )
    {
        static constexpr std::string_view ESCAPE{ "\\U+" };
        static constexpr size_t NB_HEX_DIGITS{ 4 };
        std::string result;
        result.reserve( value.size() );
        size_t position{ 0 };
        while( position < value.size() )
        {
            if( value.substr( position, ESCAPE.size() ) == ESCAPE
                && position + ESCAPE.size() + NB_HEX_DIGITS <= value.size() )
            {
                const auto digits =
                    value.substr( position + ESCAPE.size(), NB_HEX_DIGITS );
                unsigned int code_point{ 0 };
                if( absl::SimpleHexAtoi( digits, &code_point ) )
                {
                    append_utf8( result, code_point );
                    position += ESCAPE.size() + NB_HEX_DIGITS;
                    continue;
                }
            }
            result.push_back( value[position] );
            position++;
        }
        return result;
    }

    class DXFReader
    {
    public:
        explicit DXFReader( std::string_view filename )
            : filename_{ filename },
              file_{ geode::to_string( filename ), std::ios::binary }
        {
            geode::OpenGeodeIOMeshException::check_exception( file_.good(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[DXFInput] Cannot open file ", filename_ );
        }

        std::vector< SurfaceData > read()
        {
            // The content after the ENTITIES section is not needed
            while( const auto pair = next_pair() )
            {
                if( pair->code == 0 && pair->value == "EOF" )
                {
                    break;
                }
                if( pair->code != 0 || pair->value != "SECTION" )
                {
                    continue;
                }
                const auto section = next_pair();
                if( section && section->code == 2
                    && section->value == "ENTITIES" )
                {
                    read_entities();
                    break;
                }
            }
            std::vector< SurfaceData > result;
            for( auto& surface : surfaces_ )
            {
                if( !surface.polygons.empty() )
                {
                    result.push_back( std::move( surface ) );
                }
            }
            return result;
        }

        void log_warnings() const
        {
            if( skipped_faces_.total() > 0 )
            {
                geode::Logger::warning( "[DXFInput] ", skipped_faces_.total(),
                    " faces skipped in ", filename_, ": ",
                    skipped_faces_.degenerate,
                    " with less than 3 distinct vertices, ",
                    skipped_faces_.out_of_range,
                    " with out of range vertex indices, ",
                    skipped_faces_.repeated_vertex, " with a repeated vertex" );
            }
            if( !skipped_entities_.empty() )
            {
                std::vector< std::string > entities;
                entities.reserve( skipped_entities_.size() );
                for( const auto& [type, count] : skipped_entities_ )
                {
                    entities.push_back(
                        absl::StrCat( type, " (", count, ")" ) );
                }
                std::sort( entities.begin(), entities.end() );
                geode::Logger::warning(
                    "[DXFInput] Unsupported entities skipped "
                    "in ",
                    filename_, ": ", absl::StrJoin( entities, ", " ) );
            }
        }

    private:
        std::optional< GroupPair > next_pair()
        {
            if( pending_ )
            {
                auto pair = std::move( pending_ );
                pending_.reset();
                return pair;
            }
            std::string code_line;
            if( !std::getline( file_, code_line ) )
            {
                return std::nullopt;
            }
            line_number_++;
            auto code_text = absl::StripAsciiWhitespace( code_line );
            if( line_number_ == 1 )
            {
                absl::ConsumePrefix( &code_text, UTF8_BOM );
            }
            check_line( line_number_ != 1
                            || !absl::StartsWith( code_text, BINARY_SENTINEL ),
                "Binary DXF files are not supported" );
            GroupPair pair;
            check_line( absl::SimpleAtoi( code_text, &pair.code ),
                "invalid group code \"", code_text, "\"" );
            std::string value_line;
            check_line(
                static_cast< bool >( std::getline( file_, value_line ) ),
                "missing value of group code ", pair.code );
            line_number_++;
            pair.value =
                std::string{ absl::StripAsciiWhitespace( value_line ) };
            return pair;
        }

        std::optional< Entity > next_entity()
        {
            if( pending_entity_ )
            {
                auto entity = std::move( pending_entity_ );
                pending_entity_.reset();
                return entity;
            }
            auto start = next_pair();
            // Pairs before the first entity (e.g. 999 comments) are skipped
            while( start && start->code != 0 )
            {
                start = next_pair();
            }
            if( !start )
            {
                return std::nullopt;
            }
            Entity entity;
            entity.type = std::move( start->value );
            if( entity.type == "EOF" || entity.type == "ENDSEC" )
            {
                // Nothing is read after the end of the entities
                return entity;
            }
            while( auto pair = next_pair() )
            {
                if( pair->code == 0 )
                {
                    pending_ = std::move( pair );
                    break;
                }
                entity.groups.push_back( std::move( pair.value() ) );
            }
            return entity;
        }

        void read_entities()
        {
            while( auto entity = next_entity() )
            {
                if( entity->type == "ENDSEC" || entity->type == "EOF" )
                {
                    return;
                }
                if( entity->type == "3DFACE" )
                {
                    read_3dface( *entity );
                }
                else if( entity->type == "POLYLINE" )
                {
                    read_polyline( *entity );
                }
                else if( entity->type != "VIEWPORT" )
                {
                    // AutoCAD writes VIEWPORT entities routinely: skipped
                    // without warning
                    skipped_entities_[entity->type]++;
                }
            }
        }

        void read_3dface( const Entity& entity )
        {
            std::string layer{ "0" };
            std::array< geode::Point3D, NB_3DFACE_CORNERS > corners;
            std::array< std::array< bool, 3 >, NB_3DFACE_CORNERS > given{};
            for( const auto& group : entity.groups )
            {
                if( group.code == 8 )
                {
                    layer = decode_unicode_escapes( group.value );
                }
                else if( group.code >= 10 && group.code < 40 )
                {
                    const auto axis = group.code / 10 - 1;
                    const auto corner = group.code % 10;
                    if( corner < NB_3DFACE_CORNERS )
                    {
                        corners[static_cast< size_t >( corner )].set_value(
                            static_cast< geode::local_index_t >( axis ),
                            parse_double( group.value ) );
                        given[static_cast< size_t >( corner )]
                             [static_cast< size_t >( axis )] = true;
                    }
                }
            }
            const auto is_complete = []( const std::array< bool, 3 >& axes ) {
                return axes[0] && axes[1] && axes[2];
            };
            const auto is_absent = []( const std::array< bool, 3 >& axes ) {
                return !axes[0] && !axes[1] && !axes[2];
            };
            constexpr size_t LAST_CORNER{ NB_3DFACE_CORNERS - 1 };
            if( is_absent( given[LAST_CORNER] ) )
            {
                // A triangle: the fourth corner is the third one
                corners[LAST_CORNER] = corners[LAST_CORNER - 1];
                given[LAST_CORNER] = given[LAST_CORNER - 1];
            }
            if( !std::all_of( given.begin(), given.end(), is_complete ) )
            {
                skipped_faces_.degenerate++;
                return;
            }
            std::vector< geode::Point3D > polygon(
                corners.begin(), corners.end() );
            const auto status = clean_polygon( polygon );
            if( status != PolygonStatus::valid )
            {
                count_skipped( status, skipped_faces_ );
                return;
            }
            auto& surface = layer_surface( layer );
            std::vector< geode::index_t > vertices;
            vertices.reserve( polygon.size() );
            for( const auto& point : polygon )
            {
                vertices.push_back(
                    static_cast< geode::index_t >( surface.points.size() ) );
                surface.points.push_back( point );
            }
            surface.polygons.push_back( std::move( vertices ) );
        }

        SurfaceData& layer_surface( const std::string& layer )
        {
            const auto [it, inserted] =
                layer_surfaces_.try_emplace( layer, surfaces_.size() );
            if( inserted )
            {
                auto& surface = surfaces_.emplace_back();
                surface.name = layer;
                surface.colocate = true;
            }
            return surfaces_[it->second];
        }

        void read_polyline( const Entity& polyline )
        {
            std::string layer{ "0" };
            std::string handle;
            int flag{ 0 };
            absl::flat_hash_map< std::string, std::string > xdata;
            std::optional< std::string > application;
            for( const auto& group : polyline.groups )
            {
                if( group.code == 8 )
                {
                    layer = group.value;
                }
                else if( group.code == 5 )
                {
                    handle = group.value;
                }
                else if( group.code == 70 )
                {
                    flag = parse_int( group.value );
                }
                else if( group.code == XDATA_APPLICATION_CODE )
                {
                    application = group.value;
                }
                else if( application
                         && ( group.code == XDATA_STRING_CODE
                              || group.code == XDATA_REAL_CODE
                              || group.code == XDATA_INTEGER_CODE ) )
                {
                    xdata.try_emplace( application.value(), group.value );
                    application.reset();
                }
            }
            const auto is_polyface = ( flag & POLYFACE_MESH_FLAG ) != 0;
            SurfaceData surface;
            std::vector< std::vector< geode::index_t > > faces;
            while( auto vertex = next_entity() )
            {
                if( vertex->type == "SEQEND" )
                {
                    break;
                }
                if( vertex->type != "VERTEX" )
                {
                    // Missing SEQEND: the entity is read by the caller
                    pending_entity_ = std::move( vertex );
                    break;
                }
                if( is_polyface )
                {
                    read_polyface_vertex( *vertex, surface.points, faces );
                }
            }
            if( !is_polyface )
            {
                skipped_entities_["POLYLINE (not a polyface mesh)"]++;
                return;
            }
            surface.name = polyline_name( xdata, layer, handle );
            for( auto& face : faces )
            {
                add_polyface_polygon( surface, std::move( face ) );
            }
            surfaces_.push_back( std::move( surface ) );
        }

        void read_polyface_vertex( const Entity& vertex,
            std::vector< geode::Point3D >& points,
            std::vector< std::vector< geode::index_t > >& faces ) const
        {
            int flag{ 0 };
            geode::Point3D position;
            std::array< int, NB_FACE_INDICES > indices{ 0, 0, 0, 0 };
            for( const auto& group : vertex.groups )
            {
                if( group.code == 70 )
                {
                    flag = parse_int( group.value );
                }
                else if( group.code == 10 || group.code == 20
                         || group.code == 30 )
                {
                    position.set_value( static_cast< geode::local_index_t >(
                                            group.code / 10 - 1 ),
                        parse_double( group.value ) );
                }
                else if( group.code >= FIRST_FACE_INDEX_CODE
                         && group.code
                                < FIRST_FACE_INDEX_CODE + NB_FACE_INDICES )
                {
                    indices[static_cast< size_t >(
                        group.code - FIRST_FACE_INDEX_CODE )] =
                        parse_int( group.value );
                }
            }
            if( ( flag & POLYFACE_VERTEX_FLAG ) != 0 )
            {
                points.push_back( position );
            }
            else if( ( flag & FACE_RECORD_FLAG ) != 0 )
            {
                auto& face = faces.emplace_back();
                for( const auto index : indices )
                {
                    if( index != 0 )
                    {
                        // The sign of the index is the edge visibility.
                        // Widened: the absolute value of INT_MIN is defined.
                        face.push_back( static_cast< geode::index_t >( std::abs(
                            static_cast< std::int64_t >( index ) ) ) );
                    }
                }
            }
        }

        void add_polyface_polygon(
            SurfaceData& surface, std::vector< geode::index_t > face )
        {
            for( auto& index : face )
            {
                if( index > surface.points.size() )
                {
                    skipped_faces_.out_of_range++;
                    return;
                }
                index--;
            }
            const auto status = clean_polygon( face );
            if( status != PolygonStatus::valid )
            {
                count_skipped( status, skipped_faces_ );
                return;
            }
            surface.polygons.push_back( std::move( face ) );
        }

        static std::string polyline_name(
            const absl::flat_hash_map< std::string, std::string >& xdata,
            std::string_view layer,
            std::string_view handle )
        {
            for( const auto application : NAME_APPLICATIONS )
            {
                const auto it = xdata.find( application );
                if( it != xdata.end() && !it->second.empty() )
                {
                    return decode_unicode_escapes( it->second );
                }
            }
            auto name = decode_unicode_escapes( layer );
            if( handle.empty() )
            {
                return name;
            }
            return absl::StrCat( name, "_", handle );
        }

        [[nodiscard]] int parse_int( std::string_view value ) const
        {
            int result{ 0 };
            check_line( absl::SimpleAtoi( value, &result ),
                "invalid integer \"", value, "\"" );
            return result;
        }

        [[nodiscard]] double parse_double( std::string_view value ) const
        {
            double result{ 0 };
            check_line(
                absl::SimpleAtod( value, &result ) && std::isfinite( result ),
                "invalid real \"", value, "\"" );
            return result;
        }

        template < typename... Args >
        void check_line( bool condition, const Args&... message ) const
        {
            geode::OpenGeodeIOMeshException::check_exception( condition,
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[DXFInput] Line ", line_number_, " of file ", filename_, ": ",
                message... );
        }

    private:
        std::string_view filename_;
        std::ifstream file_;
        geode::index_t line_number_{ 0 };
        std::optional< GroupPair > pending_;
        std::optional< Entity > pending_entity_;
        std::vector< SurfaceData > surfaces_;
        absl::flat_hash_map< std::string, geode::index_t > layer_surfaces_;
        absl::flat_hash_map< std::string, geode::index_t > skipped_entities_;
        SkippedFaces skipped_faces_;
    };

    std::unique_ptr< geode::PolygonalSurface3D > build_surface(
        SurfaceData& data, const geode::MeshImpl& impl )
    {
        auto surface = geode::PolygonalSurface3D::create( impl );
        auto builder = geode::PolygonalSurfaceBuilder3D::create( *surface );
        builder->set_name( data.name );
        if( data.colocate )
        {
            const geode::NNSearch3D search{ std::move( data.points ) };
            const auto colocated =
                search.colocated_index_mapping( geode::GLOBAL_EPSILON );
            data.points = colocated.unique_points;
            for( auto& polygon : data.polygons )
            {
                for( auto& vertex : polygon )
                {
                    vertex = colocated.colocated_mapping[vertex];
                }
            }
        }
        builder->create_vertices( data.points.size() );
        for( const auto v : geode::Indices{ data.points } )
        {
            builder->set_point( v, data.points[v] );
        }
        for( auto& polygon : data.polygons )
        {
            // Colocation may merge distinct close corners
            if( clean_polygon( polygon ) == PolygonStatus::valid )
            {
                builder->create_polygon( polygon );
            }
        }
        builder->compute_polygon_adjacencies();
        return surface;
    }
} // namespace

namespace geode
{
    namespace internal
    {
        std::vector< std::unique_ptr< PolygonalSurface3D > > DXFInput::read(
            const MeshImpl& impl )
        {
            DXFReader reader{ filename() };
            auto data = reader.read();
            reader.log_warnings();
            std::vector< std::unique_ptr< PolygonalSurface3D > > surfaces;
            surfaces.reserve( data.size() );
            for( auto& surface : data )
            {
                surfaces.push_back( build_surface( surface, impl ) );
            }
            return surfaces;
        }

        Percentage DXFInput::is_loadable() const
        {
            try
            {
                const auto data = DXFReader{ filename() }.read();
                return Percentage{ data.empty() ? 0. : 1. };
            }
            catch( const OpenGeodeException& )
            {
                return Percentage{ 0 };
            }
        }
    } // namespace internal
} // namespace geode
