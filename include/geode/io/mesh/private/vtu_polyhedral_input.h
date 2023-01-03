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

#pragma once

#include <geode/mesh/io/polyhedral_solid_input.h>

namespace geode
{
    FORWARD_DECLARATION_DIMENSION_CLASS( PolyhedralSolid );
    ALIAS_3D( PolyhedralSolid );
} // namespace geode

namespace geode
{
    namespace detail
    {
        class VTUPolyhedralInput final : public PolyhedralSolidInput< 3 >
        {
        public:
            VTUPolyhedralInput( absl::string_view filename )
                : PolyhedralSolidInput< 3 >( filename )
            {
            }

            static absl::string_view extension()
            {
                static constexpr auto ext = "vtu";
                return ext;
            }

            std::unique_ptr< PolyhedralSolid3D > read(
                const MeshImpl& impl ) final;
        };
    } // namespace detail
} // namespace geode