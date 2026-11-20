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

#include <geode/io/mesh/internal/dxf_triangulated_output.hpp>

#include <array>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <string>
#include <string_view>
#include <vector>

#include <absl/strings/str_format.h>

#include <geode/geometry/point.hpp>

#include <geode/mesh/core/triangulated_surface.hpp>

#include <geode/io/mesh/common.hpp>

namespace
{
    constexpr char EOL{ '\n' };
    constexpr std::string_view DEFAULT_LAYER{ "0" };
    constexpr std::string_view FORBIDDEN_LAYER_CHARACTERS{ "<>/\\\":;?*|=`" };
    constexpr unsigned int MAX_ESCAPED_CODE_POINT{ 0xFFFF };
    constexpr unsigned char FIRST_PRINTABLE{ 0x20 };
    constexpr unsigned char FIRST_MULTI_BYTE{ 0x80 };
    constexpr unsigned char FIRST_LEADING_BYTE{ 0xC0 };
    constexpr unsigned char FIRST_3_BYTES_LEADING_BYTE{ 0xE0 };
    constexpr unsigned char FIRST_4_BYTES_LEADING_BYTE{ 0xF0 };
    constexpr unsigned char CONTINUATION_BITS{ 0x3F };
    // A triangle is a 3DFACE whose fourth corner is the third one
    constexpr std::array< geode::local_index_t, 4 > FACE_CORNERS{ 0, 1, 2, 2 };

    size_t utf8_sequence_length( unsigned char leading_byte )
    {
        if( leading_byte >= FIRST_4_BYTES_LEADING_BYTE )
        {
            return 4;
        }
        if( leading_byte >= FIRST_3_BYTES_LEADING_BYTE )
        {
            return 3;
        }
        if( leading_byte >= FIRST_LEADING_BYTE )
        {
            return 2;
        }
        // Unexpected continuation byte
        return 1;
    }

    /*!
     * Layer names cannot contain some characters, which are replaced by '_'.
     * Non-ASCII characters are written as \U+XXXX escapes.
     */
    std::string layer_name( std::string_view name )
    {
        if( name.empty() )
        {
            return std::string{ DEFAULT_LAYER };
        }
        std::string layer;
        layer.reserve( name.size() );
        size_t position{ 0 };
        while( position < name.size() )
        {
            const auto byte = static_cast< unsigned char >( name[position] );
            if( byte < FIRST_MULTI_BYTE )
            {
                const auto is_valid =
                    byte >= FIRST_PRINTABLE
                    && FORBIDDEN_LAYER_CHARACTERS.find( name[position] )
                           == std::string_view::npos;
                layer.push_back( is_valid ? name[position] : '_' );
                position++;
                continue;
            }
            const auto length = utf8_sequence_length( byte );
            if( length == 1 || position + length > name.size() )
            {
                layer.push_back( '_' );
                position++;
                continue;
            }
            unsigned int code_point = byte & ( 0x7FU >> length );
            for( const auto i : geode::Range{ 1, length } )
            {
                code_point =
                    ( code_point << 6U )
                    | ( static_cast< unsigned char >( name[position + i] )
                        & CONTINUATION_BITS );
            }
            if( code_point > MAX_ESCAPED_CODE_POINT )
            {
                layer.push_back( '_' );
            }
            else
            {
                absl::StrAppendFormat( &layer, "\\U+%04X", code_point );
            }
            position += length;
        }
        return layer;
    }

    class DXFTriangulatedWriter
    {
    public:
        DXFTriangulatedWriter( std::string_view filename,
            const geode::TriangulatedSurface3D& mesh )
            : filename_{ filename },
              file_{ geode::to_string( filename ) },
              mesh_( mesh )
        {
            geode::OpenGeodeIOMeshException::check_exception( file_.good(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[DXFTriangulatedOutput] Error while opening file: ",
                filename_ );
            file_.imbue( std::locale::classic() );
            file_.precision( std::numeric_limits< double >::max_digits10 );
        }

        void write()
        {
            write_group( 0, "SECTION" );
            write_group( 2, "HEADER" );
            write_group( 9, "$ACADVER" );
            write_group( 1, "AC1009" );
            write_group( 0, "ENDSEC" );
            write_group( 0, "SECTION" );
            write_group( 2, "ENTITIES" );
            const auto layer = layer_name( mesh_.name().value_or( "" ) );
            for( const auto triangle : geode::Range{ mesh_.nb_polygons() } )
            {
                write_face( triangle, layer );
            }
            write_group( 0, "ENDSEC" );
            write_group( 0, "EOF" );
            file_ << std::flush;
            geode::OpenGeodeIOMeshException::check_exception( file_.good(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[DXFTriangulatedOutput] Error while writing file: ",
                filename_ );
        }

    private:
        template < typename Value >
        void write_group( int code, const Value& value )
        {
            file_ << std::setw( 3 ) << code << EOL << value << EOL;
        }

        void write_face( geode::index_t triangle, const std::string& layer )
        {
            write_group( 0, "3DFACE" );
            write_group( 8, layer );
            for( const auto corner : geode::LIndices{ FACE_CORNERS } )
            {
                const auto& point = mesh_.point( mesh_.polygon_vertex(
                    { triangle, FACE_CORNERS[corner] } ) );
                for( const auto axis : geode::LRange{ 3 } )
                {
                    write_group(
                        10 * ( axis + 1 ) + corner, point.value( axis ) );
                }
            }
        }

    private:
        std::string_view filename_;
        std::ofstream file_;
        const geode::TriangulatedSurface3D& mesh_;
    };
} // namespace

namespace geode
{
    namespace internal
    {
        std::vector< std::string > DXFTriangulatedOutput::write(
            const TriangulatedSurface3D& surface ) const
        {
            DXFTriangulatedWriter writer{ filename(), surface };
            writer.write();
            return { to_string( filename() ) };
        }
    } // namespace internal
} // namespace geode
