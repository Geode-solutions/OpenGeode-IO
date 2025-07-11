/*
 * Copyright (c) 2019 - 2025 Geode-solutions
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

#include <geode/mesh/io/tetrahedral_solid_input.hpp>

namespace geode
{
    FORWARD_DECLARATION_DIMENSION_CLASS( TetrahedralSolid );
    ALIAS_3D( TetrahedralSolid );
} // namespace geode

namespace geode
{
    namespace detail
    {
        class VTUTetrahedralInput final : public TetrahedralSolidInput< 3 >
        {
        public:
            explicit VTUTetrahedralInput( std::string_view filename )
                : TetrahedralSolidInput< 3 >( filename )
            {
            }

            static std::string_view extension()
            {
                static constexpr auto EXT = "vtu";
                return EXT;
            }

            std::unique_ptr< TetrahedralSolid3D > read(
                const MeshImpl& impl ) final;

            bool is_loadable() const final;
        };
    } // namespace detail
} // namespace geode