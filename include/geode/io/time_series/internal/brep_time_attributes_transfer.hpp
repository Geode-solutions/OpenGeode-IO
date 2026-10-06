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

#include <absl/container/flat_hash_set.h>
#include <absl/types/span.h>

#include <geode/io/time_series/common.hpp>
#include <geode/io/time_series/internal/brep_blocks_matcher.hpp>

namespace geode
{
    FORWARD_DECLARATION_DIMENSION_CLASS( SolidMesh );
    ALIAS_3D( SolidMesh );
    class AttributeManager;
    class BRep;
} // namespace geode

namespace geode
{
    namespace internal
    {

        class BRepTimeAttributesTransfer
        {
        public:
            BRepTimeAttributesTransfer( BRep& brep,
                absl::Span< const std::string_view > ignored_attributes );

            void write_step( double time,
                const SolidMesh3D& mesh,
                const SolidToBlocksMappings& mappings );

        private:
            void write_vertex_step( double time,
                const SolidMesh3D& mesh,
                const absl::flat_hash_map< uuid, GenericMapping< index_t > >&
                    block_mappings );

            void write_polyhedron_step( double time,
                const SolidMesh3D& mesh,
                const absl::flat_hash_map< uuid, GenericMapping< index_t > >&
                    block_mappings );

            void write_block_step( double time,
                const AttributeManager& manager,
                AttributeManager& block_manager,
                const GenericMapping< index_t >& mesh2block );

        private:
            BRep& brep_;
            absl::flat_hash_set< std::string > ignored_attributes_;
        };
    } // namespace internal
} // namespace geode
