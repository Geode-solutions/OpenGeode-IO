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

#include <geode/io/time_series/internal/pvd_attributes_transfer.hpp>

#include <vector>

#include <absl/container/flat_hash_map.h>

#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/core/solid_mesh.hpp>

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
            geode::Logger::warning( "[PVDAttributesTransfer] Attribute ", name,
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
    PVDAttributesTransfer::PVDAttributesTransfer(
        absl::Span< const std::string_view > ignored_attributes )
        : ignored_attributes_(
              ignored_attributes.begin(), ignored_attributes.end() )
    {
    }

    void PVDAttributesTransfer::write_step( double time,
        absl::Span< const std::unique_ptr< SolidMesh3D > > meshes,
        const PVDPointSets& point_sets )
    {
        const auto& mappings = point_sets.mappings;
        OpenGeodeIOTimeSeriesException::check_exception(
            meshes.size() == mappings.size(), nullptr,
            OpenGeodeException::TYPE::internal,
            "[PVDAttributesTransfer::write_step] Each dataset should "
            "have its mappings" );
        std::vector< DatasetElements > vertices;
        std::vector< DatasetElements > polyhedra;
        for( const auto dataset : Indices{ meshes } )
        {
            const auto& mesh = *meshes[dataset];
            const auto& dataset_mappings = mappings[dataset];
            vertices.push_back( { mesh.vertex_attribute_manager(),
                point_sets.unique_vertices->vertex_attribute_manager(),
                dataset_mappings.vertices } );
            polyhedra.push_back( { mesh.polyhedron_attribute_manager(),
                point_sets.cell_barycenters->vertex_attribute_manager(),
                dataset_mappings.polyhedra } );
        }
        write_elements_step( time, vertices );
        write_elements_step( time, polyhedra );
    }

    void PVDAttributesTransfer::write_elements_step(
        double time, absl::Span< const DatasetElements > datasets )
    {
        const auto names = unify_attribute_ids( datasets );
        for( const auto& dataset : datasets )
        {
            for( const auto& name : names )
            {
                delete_existing_step( dataset.target, name, time );
            }
        }
        for( const auto& dataset : datasets )
        {
            write_dataset_step( time, dataset );
        }
    }

    // Each dataset gives its own id to a field: the ids are made identical
    // so that every dataset fills the same time step attribute of a Block.
    // Returns the names of the transferred attributes.
    absl::flat_hash_set< std::string >
        PVDAttributesTransfer::unify_attribute_ids(
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

    std::optional< std::string > PVDAttributesTransfer::transferred_name(
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

    void PVDAttributesTransfer::write_dataset_step(
        double time, const DatasetElements& dataset ) const
    {
        const auto& manager = dataset.manager;
        for( const auto& attribute_id : manager.attribute_ids() )
        {
            if( !transferred_name( manager, attribute_id ) )
            {
                continue;
            }
            auto properties =
                manager.find_generic_attribute( attribute_id )->properties();
            properties.time = time;
            dataset.target.import( manager, dataset.mapping, attribute_id );
            dataset.target.set_attribute_properties( attribute_id, properties );
        }
    }
} // namespace geode::internal
