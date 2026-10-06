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

#include <geode/io/time_series/internal/brep_time_attributes_transfer.hpp>

#include <array>

#include <absl/algorithm/container.h>

#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/logger.hpp>

#include <geode/mesh/core/solid_mesh.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/representation/core/brep.hpp>

namespace
{
    // GEOS parallel partitioning bookkeeping arrays, meaningless on the BRep.
    // Mesh internal attributes (points, polyhedra around vertex...) are not
    // listed: they are not transferable, unlike the arrays read from the file
    constexpr std::array< std::string_view, 2 > IGNORED_ATTRIBUTES{
        "localToGlobalMap", "ghostRank"
    };

    void delete_existing_step(
        geode::AttributeManager& manager, std::string_view name, double time )
    {
        for( const auto& step : manager.time_steps( name ) )
        {
            if( step.time == time )
            {
                manager.delete_attribute( step.attribute_id );
            }
        }
    }

} // namespace

namespace geode
{
    namespace internal
    {
        BRepTimeAttributesTransfer::BRepTimeAttributesTransfer(
            const BRep& brep )
            : brep_( brep )
        {
        }

        void BRepTimeAttributesTransfer::write_step( double time,
            const SolidMesh3D& mesh,
            const ModelToSolidMappings& mappings )
        {
            write_vertex_step( time, mesh, mappings.vertices );
            write_polyhedron_step( time, mesh, mappings.polyhedra );
        }

        void BRepTimeAttributesTransfer::write_vertex_step( double time,
            const SolidMesh3D& mesh,
            const absl::flat_hash_map< uuid, GenericMapping< index_t > >&
                block_mappings )
        {
            for( const auto& [block_id, mesh2block] : block_mappings )
            {
                write_block_step( time, mesh.vertex_attribute_manager(),
                    brep_.block( block_id ).mesh().vertex_attribute_manager(),
                    mesh2block );
            }
        }

        void BRepTimeAttributesTransfer::write_polyhedron_step( double time,
            const SolidMesh3D& mesh,
            const absl::flat_hash_map< uuid, GenericMapping< index_t > >&
                block_mappings )
        {
            for( const auto& [block_id, mesh2block] : block_mappings )
            {
                write_block_step( time, mesh.polyhedron_attribute_manager(),
                    brep_.block( block_id )
                        .mesh()
                        .polyhedron_attribute_manager(),
                    mesh2block );
            }
        }

        void BRepTimeAttributesTransfer::write_block_step( double time,
            const AttributeManager& manager,
            AttributeManager& block_manager,
            const GenericMapping< index_t >& mesh2block )
        {
            for( const auto& attribute_id : manager.attribute_ids() )
            {
                const auto attribute =
                    manager.find_generic_attribute( attribute_id );
                const auto& name = attribute->name();
                if( !name || !attribute->properties().transferable
                    || absl::c_linear_search(
                        IGNORED_ATTRIBUTES, name.value() ) )
                {
                    continue;
                }
                delete_existing_step( block_manager, name.value(), time );
                auto properties = attribute->properties();
                properties.time = time;
                block_manager.import( manager, mesh2block, attribute_id );
                block_manager.set_attribute_properties(
                    attribute_id, properties );
            }
        }
    } // namespace internal
} // namespace geode
