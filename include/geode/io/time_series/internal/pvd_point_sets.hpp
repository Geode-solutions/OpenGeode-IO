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
#include <vector>

#include <absl/types/span.h>

#include <geode/basic/mapping.hpp>

#include <geode/mesh/core/point_set.hpp>

#include <geode/io/time_series/common.hpp>

namespace geode
{
    FORWARD_DECLARATION_DIMENSION_CLASS( SolidMesh );
    ALIAS_3D( SolidMesh );
} // namespace geode

namespace geode::internal
{
    // Vertices of a .pvd dataset to unique vertices, its polyhedra to cell
    // barycenters
    struct PVDDatasetMappings
    {
        GenericMapping< index_t > vertices;
        GenericMapping< index_t > polyhedra;
    };

    // Point sets of a .pvd time series, given to the ExternalTimeSeriesHelpers
    // once all the time steps are written
    struct PVDPointSets
    {
        std::unique_ptr< PointSet3D > unique_vertices;
        std::unique_ptr< PointSet3D > cell_barycenters;
        std::vector< PVDDatasetMappings > mappings;
    };

    /*!
     * Create the .pvd point sets from the datasets of a time step
     * (e.g. one per GEOS region and per MPI rank): the dataset vertices,
     * colocated ones merged, and the dataset polyhedron barycenters, with
     * the mappings of each dataset to them.
     */
    [[nodiscard]] PVDPointSets create_pvd_point_sets(
        absl::Span< const std::unique_ptr< SolidMesh3D > > meshes );
} // namespace geode::internal
