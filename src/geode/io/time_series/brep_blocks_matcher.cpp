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

#include <geode/io/time_series/internal/brep_blocks_matcher.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <absl/container/flat_hash_set.h>

#include <geode/basic/logger.hpp>

#include <geode/geometry/distance.hpp>
#include <geode/geometry/point.hpp>

#include <geode/mesh/core/solid_mesh.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/mixin/core/vertex_identifier.hpp>
#include <geode/model/representation/core/brep.hpp>

namespace
{

    std::tuple< geode::index_t, double > closest_polyhedron_vertex(
        const geode::SolidMesh3D& solid_mesh,
        geode::index_t polyhedron,
        const geode::Point3D& point )
    {
        auto closest = geode::NO_ID;
        auto min_distance = std::numeric_limits< double >::max();
        for( const auto v :
            geode::LRange{ solid_mesh.nb_polyhedron_vertices( polyhedron ) } )
        {
            const auto vertex =
                solid_mesh.polyhedron_vertex( { polyhedron, v } );
            const auto distance = geode::point_point_distance(
                point, solid_mesh.point( vertex ) );
            if( distance < min_distance )
            {
                min_distance = distance;
                closest = vertex;
            }
        }
        return { closest, min_distance };
    }

    void map_block_vertices( geode::internal::SolidToBlocksMappings& mappings,
        absl::Span< const geode::ComponentMeshVertex > component_vertices,
        geode::index_t solid_vertex )
    {
        for( const auto& component_vertex : component_vertices )
        {
            if( component_vertex.component_id.type
                == geode::Block3D::component_type_static() )
            {
                mappings.vertices[component_vertex.component_id.id].map(
                    solid_vertex, component_vertex.vertex );
            }
        }
    }
} // namespace

namespace geode::internal
{
    BRepBlocksMatcher::BRepBlocksMatcher( const BRep& brep )
        : brep_( brep ), barycenters_{ compute_barycenters() }
    {
    }

    SolidToBlocksMappings BRepBlocksMatcher::mappings(
        const SolidMesh3D& solid ) const
    {
        const auto matched_ids = match_polyhedra( solid );
        return build_mappings( solid, matched_ids );
    }

    std::vector< Point3D > BRepBlocksMatcher::compute_barycenters()
    {
        std::vector< Point3D > barycenters;
        for( const auto& block : brep_.blocks() )
        {
            const auto& mesh = block.mesh();
            for( const auto p : Range{ mesh.nb_polyhedra() } )
            {
                polyhedra_.push_back( { block.id(), p } );
                barycenters.push_back( mesh.polyhedron_barycenter( p ) );
            }
        }
        return barycenters;
    }

    std::vector< index_t > BRepBlocksMatcher::match_polyhedra(
        const SolidMesh3D& solid ) const
    {
        std::vector< index_t > matched_ids;
        matched_ids.reserve( solid.nb_polyhedra() );
        for( const auto p : Range{ solid.nb_polyhedra() } )
        {
            const auto barycenter = solid.polyhedron_barycenter( p );
            const auto matched_id = barycenters_.closest_neighbor( barycenter );
            matched_ids.push_back( matched_id );
        }
        return matched_ids;
    }

    SolidToBlocksMappings BRepBlocksMatcher::build_mappings(
        const SolidMesh3D& solid,
        absl::Span< const index_t > matched_ids ) const
    {
        SolidToBlocksMappings mappings;
        absl::flat_hash_set< index_t > mapped_unique_vertices;
        for( const auto p : Range{ solid.nb_polyhedra() } )
        {
            const auto matched_id = matched_ids[p];
            const auto& block_polyhedron = polyhedra_[matched_id];
            const auto& block_id = block_polyhedron.block_id;
            mappings.polyhedra[block_id].map( p, block_polyhedron.element );
            const auto& block = brep_.block( block_id );
            const auto& block_mesh = block.mesh();
            for( const auto v : LRange{ block_mesh.nb_polyhedron_vertices(
                     block_polyhedron.element ) } )
            {
                const auto block_vertex = block_mesh.polyhedron_vertex(
                    { block_polyhedron.element, v } );
                const auto unique_vertex = brep_.unique_vertex(
                    { block.component_id(), block_vertex } );
                if( !mapped_unique_vertices.insert( unique_vertex ).second )
                {
                    continue;
                }
                const auto& point = block_mesh.point( block_vertex );
                const auto [solid_vertex, distance] =
                    closest_polyhedron_vertex( solid, p, point );
                map_block_vertices( mappings,
                    brep_.component_mesh_vertices( unique_vertex ),
                    solid_vertex );
            }
        }
        return mappings;
    }
} // namespace geode::internal
