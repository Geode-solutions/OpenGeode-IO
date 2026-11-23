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

#include <optional>

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
    // 0xC0 and 0xC1 can only start overlong sequences
    constexpr unsigned int FIRST_LEADING_BYTE{ 0xC2 };
    constexpr unsigned int FIRST_3_BYTES_LEADING_BYTE{ 0xE0 };
    constexpr unsigned int FIRST_4_BYTES_LEADING_BYTE{ 0xF0 };
    // Above 0xF4, sequences encode code points beyond U+10FFFF
    constexpr unsigned int LAST_LEADING_BYTE{ 0xF4 };
    constexpr unsigned int CONTINUATION_MASK{ 0xC0 };
    constexpr unsigned int CONTINUATION_TAG{ 0x80 };
    constexpr unsigned int CONTINUATION_BITS{ 0x3F };
    constexpr unsigned int FIRST_2_BYTES_CODE_POINT{ 0x80 };
    constexpr unsigned int FIRST_3_BYTES_CODE_POINT{ 0x800 };
    constexpr unsigned int FIRST_4_BYTES_CODE_POINT{ 0x10000 };
    constexpr unsigned int FIRST_SURROGATE{ 0xD800 };
    constexpr unsigned int LAST_SURROGATE{ 0xDFFF };
    constexpr unsigned int LAST_CODE_POINT{ 0x10FFFF };

    struct UTF8Character
    {
        unsigned int code_point;
        size_t length;
    };

    /*!
     * Decodes the multi-byte UTF-8 sequence starting at position.
     * Returns nothing if it is invalid: bad leading or continuation byte,
     * truncated sequence, overlong encoding, surrogate or out of range code
     * point.
     */
    std::optional< UTF8Character > decode_utf8_sequence(
        std::string_view utf8, size_t position )
    {
        const auto leading_byte =
            static_cast< unsigned char >( utf8[position] );
        if( leading_byte < FIRST_LEADING_BYTE
            || leading_byte > LAST_LEADING_BYTE )
        {
            return std::nullopt;
        }
        size_t length{ 2 };
        unsigned int min_code_point{ FIRST_2_BYTES_CODE_POINT };
        if( leading_byte >= FIRST_4_BYTES_LEADING_BYTE )
        {
            length = 4;
            min_code_point = FIRST_4_BYTES_CODE_POINT;
        }
        else if( leading_byte >= FIRST_3_BYTES_LEADING_BYTE )
        {
            length = 3;
            min_code_point = FIRST_3_BYTES_CODE_POINT;
        }
        if( position + length > utf8.size() )
        {
            return std::nullopt;
        }
        unsigned int code_point = leading_byte & ( 0x7FU >> length );
        for( const auto i : geode::Range{ 1, length } )
        {
            const auto byte =
                static_cast< unsigned char >( utf8[position + i] );
            if( ( byte & CONTINUATION_MASK ) != CONTINUATION_TAG )
            {
                return std::nullopt;
            }
            code_point = ( code_point << 6U ) | ( byte & CONTINUATION_BITS );
        }
        if( code_point < min_code_point || code_point > LAST_CODE_POINT
            || ( code_point >= FIRST_SURROGATE
                 && code_point <= LAST_SURROGATE ) )
        {
            return std::nullopt;
        }
        return UTF8Character{ code_point, length };
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
                const auto character = decode_utf8_sequence( utf8, position );
                if( !character )
                {
                    // Only the first byte is replaced: the next ones may
                    // start a valid character
                    result.push_back( REPLACEMENT );
                    position++;
                    continue;
                }
                if( character->code_point > MAX_ESCAPED_CODE_POINT )
                {
                    result.push_back( REPLACEMENT );
                }
                else
                {
                    absl::StrAppendFormat(
                        &result, "\\U+%04X", character->code_point );
                }
                position += character->length;
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
