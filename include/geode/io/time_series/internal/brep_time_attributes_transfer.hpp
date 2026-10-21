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

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <vector>

#include <absl/container/flat_hash_set.h>
#include <absl/types/span.h>

#include <geode/io/time_series/common.hpp>
#include <geode/io/time_series/internal/brep_blocks_matcher.hpp>

namespace geode
{
    FORWARD_DECLARATION_DIMENSION_CLASS( SolidMesh );
    ALIAS_3D( SolidMesh );
    class AttributeManager;
    class ExternalTimeSeriesHelpers;
} // namespace geode

namespace geode::internal
{
    class BRepTimeAttributesTransfer
    {
        // Helpers elements filled by a dataset and the mapping to them
        struct MappedManager
        {
            AttributeManager& manager;
            const GenericMapping< index_t >& mapping;
        };

        // Elements of one dataset of a time step and the helpers elements
        // they fill
        struct DatasetElements
        {
            AttributeManager& manager;
            std::vector< MappedManager > targets;
        };

    public:
        BRepTimeAttributesTransfer( ExternalTimeSeriesHelpers& helpers,
            absl::Span< const std::string_view > ignored_attributes );

        /*!
         * Write the attributes of all the datasets of a time step (e.g. one
         * per GEOS region and per MPI rank) as time step attributes of the
         * ExternalTimeSeriesHelpers (unique vertices and Block polyhedra).
         * Attributes sharing a name across the datasets are merged into a
         * single time step attribute.
         * @warning The attributes of the given meshes are modified: those
         * sharing a name are given the same id.
         */
        void write_step( double time,
            absl::Span< const std::unique_ptr< SolidMesh3D > > meshes,
            absl::Span< const SolidToBlocksMappings > mappings );

    private:
        void write_elements_step(
            double time, absl::Span< const DatasetElements > datasets );

        [[nodiscard]] absl::flat_hash_set< std::string > unify_attribute_ids(
            absl::Span< const DatasetElements > datasets );

        [[nodiscard]] std::optional< std::string > transferred_name(
            const AttributeManager& manager, const uuid& attribute_id ) const;

        void write_target_step( double time,
            const AttributeManager& manager,
            const MappedManager& target );

    private:
        ExternalTimeSeriesHelpers& helpers_;
        absl::flat_hash_set< std::string > ignored_attributes_;
    };
} // namespace geode::internal
