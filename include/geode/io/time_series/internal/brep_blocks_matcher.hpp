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

#include <vector>

#include <absl/container/flat_hash_map.h>
#include <absl/types/span.h>

#include <geode/basic/mapping.hpp>
#include <geode/basic/uuid.hpp>

#include <geode/geometry/nn_search.hpp>

#include <geode/io/time_series/common.hpp>

namespace geode
{
    FORWARD_DECLARATION_DIMENSION_CLASS( SolidMesh );
    ALIAS_3D( SolidMesh );
    class BRep;
} // namespace geode

namespace geode::internal
{
    // Solid elements to Block elements, per Block id
    struct SolidToBlocksMappings
    {
        absl::flat_hash_map< uuid, GenericMapping< index_t > > polyhedra;
        absl::flat_hash_map< uuid, GenericMapping< index_t > > vertices;
    };

    class BRepBlocksMatcher
    {
        struct BlockElement
        {
            uuid block_id;
            index_t element;
        };

    public:
        explicit BRepBlocksMatcher( const BRep& brep );

        [[nodiscard]] SolidToBlocksMappings mappings(
            const SolidMesh3D& solid ) const;

    private:
        [[nodiscard]] std::vector< Point3D > compute_barycenters();

        [[nodiscard]] std::vector< index_t > match_polyhedra(
            const SolidMesh3D& solid ) const;

        [[nodiscard]] SolidToBlocksMappings build_mappings(
            const SolidMesh3D& solid,
            absl::Span< const index_t > matched_ids ) const;

    private:
        const BRep& brep_;
        std::vector< BlockElement > polyhedra_;
        NNSearch3D barycenters_;
    };
} // namespace geode::internal
