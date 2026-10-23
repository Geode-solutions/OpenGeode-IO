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

#include <absl/hash/hash.h>

#include <geode/basic/common.hpp>

#include <geode/model/mixin/core/component_type.hpp>

namespace geode
{
    namespace internal
    {
        struct GmshElementID
        {
            GmshElementID() = default;
            GmshElementID( ComponentType gmsh_type, index_t gmsh_id )
                : type( std::move( gmsh_type ) ), id( gmsh_id )
            {
            }

            bool operator==( const GmshElementID& other ) const
            {
                return type == other.type && id == other.id;
            }

            template < typename H >
            friend H AbslHashValue( H h, const GmshElementID& gmsh_id )
            {
                return H::combine( std::move( h ), gmsh_id.type, gmsh_id.id );
            }

            ComponentType type;
            index_t id{ NO_ID };
        };

        static constexpr index_t GMSH_OFFSET_START{ 1 };
    } // namespace internal
} // namespace geode
