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

#include <string_view>

#include <geode/basic/common.hpp>

namespace geode
{
    FORWARD_DECLARATION_DIMENSION_CLASS( SurfaceMesh );
    ALIAS_3D( SurfaceMesh );
} // namespace geode

namespace geode
{
    namespace internal
    {
        /*!
         * Writes an ASCII DXF (R12) with one 3DFACE per polygon, on a layer
         * named after the surface. A triangle is a 3DFACE whose fourth corner
         * is the third one. Polygons with more than 4 vertices cannot be
         * written: a data exception is raised before creating the file.
         *
         * The layer name is not strictly R12: R12 only allows up to 31
         * characters among A-Z 0-9 $ - _, whereas the surface name is kept
         * (spaces, case, length, non-ASCII characters as \U+XXXX escapes),
         * only characters forbidden by all DXF versions being replaced by '_'.
         * Lenient readers, including the OpenGeode-IO DXF reader, accept it.
         * A newer version (e.g. AC1015) is not declared: it requires handles
         * and complete TABLES, BLOCKS and OBJECTS sections.
         */
        void write_dxf_surface(
            std::string_view filename, const SurfaceMesh3D& surface );
    } // namespace internal
} // namespace geode
