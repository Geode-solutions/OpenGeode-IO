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

#include <geode/io/time_series/internal/pvd_point_sets.hpp>

#include <geode/basic/range.hpp>

#include <geode/geometry/nn_search.hpp>
#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/point_set_builder.hpp>
#include <geode/mesh/core/solid_mesh.hpp>

namespace
{
    std::unique_ptr< geode::PointSet3D > create_point_set(
        absl::Span< const geode::Point3D > points )
    {
        auto point_set = geode::PointSet3D::create();
        auto builder = geode::PointSetBuilder3D::create( *point_set );
        for( const auto& point : points )
        {
            builder->create_point( point );
        }
        return point_set;
    }

    // GEOS writes the vertices shared by several regions or MPI ranks in
    // each of their datasets
    std::unique_ptr< geode::PointSet3D > create_unique_vertices(
        absl::Span< const std::unique_ptr< geode::SolidMesh3D > > meshes,
        absl::Span< geode::internal::PVDDatasetMappings > mappings )
    {
        std::vector< geode::Point3D > points;
        for( const auto& mesh : meshes )
        {
            for( const auto vertex : geode::Range{ mesh->nb_vertices() } )
            {
                points.push_back( mesh->point( vertex ) );
            }
        }
        const auto colocated_info =
            geode::NNSearch3D{ std::move( points ) }.colocated_index_mapping(
                geode::GLOBAL_EPSILON );
        geode::index_t point{ 0 };
        for( const auto dataset : geode::Indices{ meshes } )
        {
            auto& vertices = mappings[dataset].vertices;
            for( const auto vertex :
                geode::Range{ meshes[dataset]->nb_vertices() } )
            {
                vertices.map(
                    vertex, colocated_info.colocated_mapping[point++] );
            }
        }
        return create_point_set( colocated_info.unique_points );
    }

    std::unique_ptr< geode::PointSet3D > create_cell_barycenters(
        absl::Span< const std::unique_ptr< geode::SolidMesh3D > > meshes,
        absl::Span< geode::internal::PVDDatasetMappings > mappings )
    {
        std::vector< geode::Point3D > barycenters;
        for( const auto dataset : geode::Indices{ meshes } )
        {
            const auto& mesh = *meshes[dataset];
            auto& polyhedra = mappings[dataset].polyhedra;
            for( const auto polyhedron : geode::Range{ mesh.nb_polyhedra() } )
            {
                polyhedra.map( polyhedron, barycenters.size() );
                barycenters.push_back(
                    mesh.polyhedron_barycenter( polyhedron ) );
            }
        }
        return create_point_set( barycenters );
    }
} // namespace

namespace geode::internal
{
    PVDPointSets create_pvd_point_sets(
        absl::Span< const std::unique_ptr< SolidMesh3D > > meshes )
    {
        PVDPointSets point_sets;
        point_sets.mappings.resize( meshes.size() );
        const auto mappings = absl::MakeSpan( point_sets.mappings );
        point_sets.unique_vertices = create_unique_vertices( meshes, mappings );
        point_sets.cell_barycenters =
            create_cell_barycenters( meshes, mappings );
        return point_sets;
    }
} // namespace geode::internal
