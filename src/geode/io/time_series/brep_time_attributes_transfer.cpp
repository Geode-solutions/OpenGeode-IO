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

#include <vector>

#include <absl/container/flat_hash_map.h>

#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/mesh/core/solid_mesh.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/representation/core/brep.hpp>

namespace
{
    struct SharedAttribute
    {
        geode::uuid id;
        std::string_view type;
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

    void unify_attribute_id( geode::AttributeManager& manager,
        const geode::uuid& attribute_id,
        const std::string& name,
        absl::flat_hash_map< std::string, SharedAttribute >& shared_attributes )
    {
        const auto type = manager.attribute_type( attribute_id );
        const auto [shared, inserted] = shared_attributes.try_emplace(
            name, SharedAttribute{ attribute_id, type } );
        if( inserted )
        {
            return;
        }
        if( shared->second.type != type )
        {
            geode::Logger::warning( "[BRepTimeAttributesTransfer] Attribute ",
                name,
                " has different types among the datasets of a time step, "
                "values of type ",
                type, " are not imported" );
            manager.delete_attribute( attribute_id );
            return;
        }
        manager.copy_attribute( attribute_id, shared->second.id );
        manager.delete_attribute( attribute_id );
    }

} // namespace

namespace geode::internal
{
    BRepTimeAttributesTransfer::BRepTimeAttributesTransfer(
        BRep& brep, absl::Span< const std::string_view > ignored_attributes )
        : brep_( brep ),
          ignored_attributes_(
              ignored_attributes.begin(), ignored_attributes.end() )
    {
    }

    void BRepTimeAttributesTransfer::write_step( double time,
        absl::Span< const std::unique_ptr< SolidMesh3D > > meshes,
        absl::Span< const SolidToBlocksMappings > mappings )
    {
        OpenGeodeIOTimeSeriesException::check_exception(
            meshes.size() == mappings.size(), nullptr,
            OpenGeodeException::TYPE::internal,
            "[BRepTimeAttributesTransfer::write_step] Each dataset should "
            "have its mappings" );
        std::vector< DatasetElements > vertices;
        std::vector< DatasetElements > polyhedra;
        for( const auto dataset : Indices{ meshes } )
        {
            const auto& mesh = *meshes[dataset];
            vertices.push_back( { mesh.vertex_attribute_manager(),
                mappings[dataset].vertices } );
            polyhedra.push_back( { mesh.polyhedron_attribute_manager(),
                mappings[dataset].polyhedra } );
        }
        write_elements_step( time, vertices,
            []( const SolidMesh3D& block_mesh ) -> AttributeManager& {
                return block_mesh.vertex_attribute_manager();
            } );
        write_elements_step( time, polyhedra,
            []( const SolidMesh3D& block_mesh ) -> AttributeManager& {
                return block_mesh.polyhedron_attribute_manager();
            } );
    }

    void BRepTimeAttributesTransfer::write_elements_step( double time,
        absl::Span< const DatasetElements > datasets,
        absl::FunctionRef< AttributeManager&( const SolidMesh3D& ) >
            block_manager )
    {
        const auto names = unify_attribute_ids( datasets );
        for( const auto& block : brep_.blocks() )
        {
            auto& manager = block_manager( block.mesh() );
            for( const auto& name : names )
            {
                delete_existing_step( manager, name, time );
            }
        }
        for( const auto& dataset : datasets )
        {
            for( const auto& [block_id, mesh2block] : dataset.block_mappings )
            {
                write_block_step( time, dataset.manager,
                    block_manager( brep_.block( block_id ).mesh() ),
                    mesh2block );
            }
        }
    }

    // Each dataset gives its own id to a field: the ids are made identical
    // so that every dataset fills the same time step attribute of a Block.
    // Returns the names of the transferred attributes.
    absl::flat_hash_set< std::string >
        BRepTimeAttributesTransfer::unify_attribute_ids(
            absl::Span< const DatasetElements > datasets )
    {
        absl::flat_hash_map< std::string, SharedAttribute > shared_attributes;
        for( const auto& dataset : datasets )
        {
            auto& manager = dataset.manager;
            for( const auto& attribute_id : manager.attribute_ids() )
            {
                if( const auto name =
                        transferred_name( manager, attribute_id ) )
                {
                    unify_attribute_id( manager, attribute_id, name.value(),
                        shared_attributes );
                }
            }
        }
        absl::flat_hash_set< std::string > names;
        for( const auto& shared_attribute : shared_attributes )
        {
            names.insert( shared_attribute.first );
        }
        return names;
    }

    std::optional< std::string > BRepTimeAttributesTransfer::transferred_name(
        const AttributeManager& manager, const uuid& attribute_id ) const
    {
        const auto attribute = manager.find_generic_attribute( attribute_id );
        const auto& name = attribute->name();
        if( !name || !attribute->properties().transferable
            || ignored_attributes_.contains( name.value() ) )
        {
            return std::nullopt;
        }
        return name;
    }

    void BRepTimeAttributesTransfer::write_block_step( double time,
        const AttributeManager& manager,
        AttributeManager& block_manager,
        const GenericMapping< index_t >& mesh2block )
    {
        for( const auto& attribute_id : manager.attribute_ids() )
        {
            if( !transferred_name( manager, attribute_id ) )
            {
                continue;
            }
            auto properties =
                manager.find_generic_attribute( attribute_id )->properties();
            properties.time = time;
            block_manager.import( manager, mesh2block, attribute_id );
            block_manager.set_attribute_properties( attribute_id, properties );
        }
    }
} // namespace geode::internal
