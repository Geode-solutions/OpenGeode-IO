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

#include <geode/io/mesh/internal/dxf_common.hpp>

#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>

#include <geode/basic/range.hpp>

namespace
{
    constexpr std::string_view ESCAPE{ "\\U+" };
    constexpr size_t NB_HEX_DIGITS{ 4 };
    constexpr char REPLACEMENT{ '_' };
    constexpr unsigned int MAX_ESCAPED_CODE_POINT{ 0xFFFF };
    constexpr unsigned int FIRST_MULTI_BYTE{ 0x80 };
    constexpr unsigned int FIRST_LEADING_BYTE{ 0xC0 };
    constexpr unsigned int FIRST_3_BYTES_LEADING_BYTE{ 0xE0 };
    constexpr unsigned int FIRST_4_BYTES_LEADING_BYTE{ 0xF0 };
    constexpr unsigned int CONTINUATION_BITS{ 0x3F };
    constexpr unsigned int FIRST_2_BYTES_CODE_POINT{ 0x80 };
    constexpr unsigned int FIRST_3_BYTES_CODE_POINT{ 0x800 };

    size_t utf8_sequence_length( unsigned int leading_byte )
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

    void append_utf8( std::string& output, unsigned int code_point )
    {
        if( code_point < FIRST_2_BYTES_CODE_POINT )
        {
            output.push_back( static_cast< char >( code_point ) );
        }
        else if( code_point < FIRST_3_BYTES_CODE_POINT )
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
} // namespace

namespace geode
{
    namespace internal
    {
        std::string encode_unicode_escapes( std::string_view utf8 )
        {
            std::string result;
            result.reserve( utf8.size() );
            size_t position{ 0 };
            while( position < utf8.size() )
            {
                const auto byte =
                    static_cast< unsigned char >( utf8[position] );
                if( byte < FIRST_MULTI_BYTE )
                {
                    result.push_back( utf8[position] );
                    position++;
                    continue;
                }
                const auto length = utf8_sequence_length( byte );
                if( length == 1 || position + length > utf8.size() )
                {
                    result.push_back( REPLACEMENT );
                    position++;
                    continue;
                }
                unsigned int code_point = byte & ( 0x7FU >> length );
                for( const auto i : Range{ 1, length } )
                {
                    code_point =
                        ( code_point << 6U )
                        | ( static_cast< unsigned char >( utf8[position + i] )
                            & CONTINUATION_BITS );
                }
                if( code_point > MAX_ESCAPED_CODE_POINT )
                {
                    result.push_back( REPLACEMENT );
                }
                else
                {
                    absl::StrAppendFormat( &result, "\\U+%04X", code_point );
                }
                position += length;
            }
            return result;
        }

        std::string decode_unicode_escapes( std::string_view value )
        {
            std::string result;
            result.reserve( value.size() );
            size_t position{ 0 };
            while( position < value.size() )
            {
                if( value.substr( position, ESCAPE.size() ) == ESCAPE
                    && position + ESCAPE.size() + NB_HEX_DIGITS
                           <= value.size() )
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
    } // namespace internal
} // namespace geode
