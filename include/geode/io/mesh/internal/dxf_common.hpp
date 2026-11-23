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

#include <string>
#include <string_view>

#include <geode/basic/common.hpp>

namespace geode
{
    namespace internal
    {
        /*!
         * Number of corners of a 3DFACE. A triangle repeats its third corner
         * as fourth one.
         */
        inline constexpr local_index_t NB_3DFACE_CORNERS{ 4 };

        /*!
         * Encodes the non-ASCII characters of a UTF-8 string as the \U+XXXX
         * escapes of DXF strings. Each byte of an invalid UTF-8 sequence, and
         * each code point above U+FFFF, which cannot be escaped, is replaced
         * by '_'.
         */
        [[nodiscard]] std::string encode_unicode_escapes(
            std::string_view utf8 );

        /*!
         * Decodes the \U+XXXX escapes of DXF strings to UTF-8.
         */
        [[nodiscard]] std::string decode_unicode_escapes(
            std::string_view value );
    } // namespace internal
} // namespace geode
