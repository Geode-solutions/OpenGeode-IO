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

#pragma once

#include <array>
#include <charconv>
#include <string>

#include <geode/basic/range.hpp>

#include <geode/geometry/point.hpp>

namespace geode
{
    namespace internal
    {
        /*!
         * Appends the shortest text representation of a double that reads
         * back to the same value.
         */
        inline void append_double( std::string& buffer, double value )
        {
            static constexpr std::size_t MAX_DOUBLE_LENGTH{ 32 };
            std::array< char, MAX_DOUBLE_LENGTH > chars;
            const auto result = std::to_chars(
                chars.data(), chars.data() + chars.size(), value );
            buffer.append( chars.data(), result.ptr );
        }

        /*!
         * Appends the point coordinates separated by spaces.
         */
        template < index_t dimension >
        void append_point( std::string& buffer, const Point< dimension >& point )
        {
            for( const auto d : LRange{ dimension } )
            {
                if( d != 0 )
                {
                    buffer.push_back( ' ' );
                }
                append_double( buffer, point.value( d ) );
            }
        }

        /*!
         * Returns the point coordinates separated by spaces, reusing the given
         * buffer to avoid allocations.
         */
        template < index_t dimension >
        const std::string& point_string(
            std::string& buffer, const Point< dimension >& point )
        {
            buffer.clear();
            append_point( buffer, point );
            return buffer;
        }
    } // namespace internal
} // namespace geode
