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
            explicit BRepTimeAttributesTransfer( const BRep& brep );

            void write_step( double time,
                const SolidMesh3D& mesh,
                const ModelToSolidMappings& mappings );

        private:
            void write_vertex_step( double time,
                const SolidMesh3D& mesh,
                const GenericMapping< index_t >& solid2unique );

            void write_polyhedron_step( double time,
                const SolidMesh3D& mesh,
                const absl::flat_hash_map< uuid, GenericMapping< index_t > >&
                    block_mappings );

            void write_block_step( double time,
                const AttributeManager& manager,
                AttributeManager& block_manager,
                const GenericMapping< index_t >& mesh2block );

        private:
            const BRep& brep_;
        };
    } // namespace internal
} // namespace geode
