/*
 * Copyright (c) 2019 - 2024 Geode-solutions
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

#include <fstream>
#include <mutex>
#include <numeric>

#include <absl/container/flat_hash_set.h>
#include <absl/strings/str_split.h>

#include <geode/basic/algorithm.h>
#include <geode/basic/common.h>
#include <geode/basic/factory.h>
#include <geode/basic/logger.h>
#include <geode/basic/string.h>
#include <geode/basic/uuid.h>

#include <geode/geometry/point.h>

#include <geode/mesh/builder/edged_curve_builder.h>
#include <geode/mesh/builder/point_set_builder.h>
#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/builder/polyhedral_solid_builder.h>
#include <geode/mesh/core/edged_curve.h>
#include <geode/mesh/core/hybrid_solid.h>
#include <geode/mesh/core/mesh_factory.h>
#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/polygonal_surface.h>

#include <geode/model/helpers/detail/build_model_boundaries.h>
#include <geode/model/mixin/core/block.h>
#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/mixin/core/surface.h>
#include <geode/model/representation/builder/brep_builder.h>
#include <geode/model/representation/core/brep.h>

namespace geode
{
    namespace detail
    {
        struct GmshElementID
        {
            GmshElementID() = default;
            GmshElementID(
                geode::ComponentType gmsh_type, geode::index_t gmsh_id )
                : type( std::move( gmsh_type ) ), id( gmsh_id )
            {
            }

            bool operator==( const GmshElementID& other ) const
            {
                return type == other.type && id == other.id;
            }
            geode::ComponentType type;
            geode::index_t id;
        };
    } // namespace detail
} // namespace geode

namespace std
{
    template <>
    struct hash< geode::detail::GmshElementID >
    {
        std::size_t operator()(
            const geode::detail::GmshElementID& gmsh_id ) const
        {
            return absl::Hash< std::string >()( gmsh_id.type.get() )
                   ^ absl::Hash< geode::index_t >()( gmsh_id.id );
        }
    };
} // namespace std

namespace geode
{
    namespace detail
    {
        static constexpr geode::index_t GMSH_OFFSET_START{ 1 };

        struct GmshId2Uuids
        {
            GmshId2Uuids() = default;

            bool contains_elementary_id(
                const GmshElementID& elementary_id ) const
            {
                return elementary_ids.find( elementary_id )
                       != elementary_ids.end();
            }

            bool contains_physical_id( const GmshElementID& physical_id ) const
            {
                return physical_ids.find( physical_id ) != physical_ids.end();
            }
            absl::flat_hash_map< GmshElementID, geode::uuid > elementary_ids;
            absl::flat_hash_map< GmshElementID, geode::uuid > physical_ids;
        };

        class GMSHElement
        {
        public:
            GMSHElement( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                geode::index_t nb_vertices,
                absl::Span< const std::string_view > vertex_ids )
                : physical_entity_id_( std::move( physical_entity_id ) ),
                  elementary_entity_id_( std::move( elementary_entity_id ) ),
                  nb_vertices_( nb_vertices ),
                  vertex_ids_str_( vertex_ids )
            {
                OPENGEODE_EXCEPTION( elementary_entity_id > 0,
                    "[GMSHElement] GMSH tag for elementary entity "
                    "(second tag) should not be null" );
                try
                {
                    read_vertex_ids();
                }
                catch( ... )
                {
                    geode::Logger::error(
                        "Wrong GMSH element number of vertices" );
                }
            }

            virtual ~GMSHElement() = default;

            virtual void add_element(
                geode::BRep& brep, GmshId2Uuids& id_map ) = 0;

            geode::index_t physical_entity_id()
            {
                return physical_entity_id_;
            }

            geode::index_t elementary_entity_id()
            {
                return elementary_entity_id_;
            }

        protected:
            geode::index_t& nb_vertices()
            {
                return nb_vertices_;
            }

            std::vector< geode::index_t >& vertex_ids()
            {
                return vertex_ids_;
            }

            void read_vertex_ids()
            {
                vertex_ids().resize( nb_vertices() );
                for( const auto n : geode::Range{ nb_vertices() } )
                {
                    vertex_ids()[n] =
                        geode::string_to_index( vertex_ids_str_[n] );
                }
            }

        private:
            geode::index_t physical_entity_id_;
            geode::index_t elementary_entity_id_;
            geode::index_t nb_vertices_;
            absl::Span< const std::string_view > vertex_ids_str_;
            std::vector< geode::index_t > vertex_ids_;
        };

        using GMSHElementFactory = geode::Factory< geode::index_t,
            GMSHElement,
            geode::index_t,
            geode::index_t,
            absl::Span< const std::string_view > >;

        class GMSHPoint : public GMSHElement
        {
        public:
            GMSHPoint( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHElement{ physical_entity_id, elementary_entity_id, 1,
                      vertex_ids }
            {
            }
            void add_element( geode::BRep& brep, GmshId2Uuids& id_map ) final
            {
                const GmshElementID cur_gmsh_id{
                    geode::Corner3D::component_type_static(),
                    elementary_entity_id()
                };
                const auto existing_id =
                    id_map.contains_elementary_id( cur_gmsh_id );
                geode::uuid corner_uuid;
                geode::BRepBuilder builder{ brep };
                if( existing_id )
                {
                    corner_uuid = id_map.elementary_ids.at( cur_gmsh_id );
                }
                else
                {
                    corner_uuid = builder.add_corner();
                    id_map.elementary_ids.insert(
                        { cur_gmsh_id, corner_uuid } );
                }

                const auto v_id =
                    builder.corner_mesh_builder( corner_uuid )->create_vertex();
                builder.set_unique_vertex(
                    { brep.corner( corner_uuid ).component_id(), v_id },
                    vertex_ids()[0] - GMSH_OFFSET_START );
            }
        };

        class GMSHEdge : public GMSHElement
        {
        public:
            GMSHEdge( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHElement{ physical_entity_id, elementary_entity_id, 2,
                      vertex_ids }
            {
            }

            void add_element( geode::BRep& brep, GmshId2Uuids& id_map ) final
            {
                const GmshElementID cur_gmsh_id(
                    geode::Line3D::component_type_static(),
                    elementary_entity_id() );
                const auto existing_id =
                    id_map.contains_elementary_id( cur_gmsh_id );
                geode::BRepBuilder builder{ brep };
                geode::uuid line_uuid;
                if( existing_id )
                {
                    line_uuid = id_map.elementary_ids.at( cur_gmsh_id );
                }
                else
                {
                    line_uuid = builder.add_line();
                    id_map.elementary_ids.insert( { cur_gmsh_id, line_uuid } );
                }
                const auto first_v_id =
                    builder.line_mesh_builder( line_uuid )
                        ->create_vertices( vertex_ids().size() );
                const auto edge_id =
                    builder.line_mesh_builder( line_uuid )
                        ->create_edge( first_v_id, first_v_id + 1 );

                const auto& line = brep.line( line_uuid );
                for( const auto v_id : geode::LIndices{ vertex_ids() } )
                {
                    builder.set_unique_vertex(
                        { line.component_id(),
                            line.mesh().edge_vertex( { edge_id, v_id } ) },
                        vertex_ids()[v_id] - GMSH_OFFSET_START );
                }
            }
        };

        class GMSHSurfacePolygon : public GMSHElement
        {
        public:
            GMSHSurfacePolygon( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                geode::index_t nb_vertices,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHElement{ physical_entity_id, elementary_entity_id,
                      nb_vertices, vertex_ids }
            {
            }

            void add_element( geode::BRep& brep, GmshId2Uuids& id_map ) final
            {
                const GmshElementID cur_gmsh_id(
                    geode::Surface3D::component_type_static(),
                    elementary_entity_id() );
                const auto existing_id =
                    id_map.contains_elementary_id( cur_gmsh_id );
                geode::BRepBuilder builder{ brep };
                geode::uuid surface_uuid;
                if( existing_id )
                {
                    surface_uuid = id_map.elementary_ids.at( cur_gmsh_id );
                }
                else
                {
                    surface_uuid = builder.add_surface();
                    id_map.elementary_ids.insert(
                        { cur_gmsh_id, surface_uuid } );
                }
                const auto first_v_id =
                    builder.surface_mesh_builder( surface_uuid )
                        ->create_vertices( vertex_ids().size() );
                std::vector< geode::index_t > v_ids( vertex_ids().size() );
                std::iota( v_ids.begin(), v_ids.end(), first_v_id );
                const auto polygon_id =
                    builder.surface_mesh_builder( surface_uuid )
                        ->create_polygon( v_ids );

                const auto& surface = brep.surface( surface_uuid );
                for( const auto v_id : geode::LIndices{ vertex_ids() } )
                {
                    builder.set_unique_vertex(
                        { surface.component_id(), surface.mesh().polygon_vertex(
                                                      { polygon_id, v_id } ) },
                        vertex_ids()[v_id] - GMSH_OFFSET_START );
                }
            }
        };

        class GMSHTriangle : public GMSHSurfacePolygon
        {
        public:
            GMSHTriangle( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHSurfacePolygon{ physical_entity_id, elementary_entity_id,
                      3, vertex_ids }
            {
            }
        };

        class GMSHQuadrangle : public GMSHSurfacePolygon
        {
        public:
            GMSHQuadrangle( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHSurfacePolygon{ physical_entity_id, elementary_entity_id,
                      4, vertex_ids }
            {
            }
        };

        class GMSHSolidPolyhedron : public GMSHElement
        {
        public:
            GMSHSolidPolyhedron( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                geode::index_t nb_vertices,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHElement{ physical_entity_id, elementary_entity_id,
                      nb_vertices, vertex_ids }
            {
            }

            virtual geode::index_t create_gmsh_polyhedron(
                geode::BRepBuilder& builder,
                const geode::uuid& block_uuid,
                const std::vector< geode::index_t >& v_ids ) = 0;

            void add_element( geode::BRep& brep, GmshId2Uuids& id_map ) final
            {
                const GmshElementID cur_gmsh_id(
                    geode::Block3D::component_type_static(),
                    elementary_entity_id() );
                const auto existing_id =
                    id_map.contains_elementary_id( cur_gmsh_id );
                geode::BRepBuilder builder{ brep };
                geode::uuid block_uuid;
                if( existing_id )
                {
                    block_uuid = id_map.elementary_ids.at( cur_gmsh_id );
                }
                else
                {
                    block_uuid =
                        builder.add_block( geode::MeshFactory::default_impl(
                            geode::HybridSolid3D::type_name_static() ) );
                    id_map.elementary_ids.insert( { cur_gmsh_id, block_uuid } );
                }

                const auto first_v_id =
                    builder.block_mesh_builder( block_uuid )
                        ->create_vertices( vertex_ids().size() );
                std::vector< geode::index_t > v_ids( vertex_ids().size() );
                std::iota( v_ids.begin(), v_ids.end(), first_v_id );
                const auto polyhedron_id =
                    create_gmsh_polyhedron( builder, block_uuid, v_ids );

                const auto& block = brep.block( block_uuid );
                for( const auto v_id : geode::LIndices{ vertex_ids() } )
                {
                    builder.set_unique_vertex(
                        { block.component_id(), block.mesh().polyhedron_vertex(
                                                    { polyhedron_id, v_id } ) },
                        vertex_ids()[v_id] - GMSH_OFFSET_START );
                }
            }
        };

        class GMSHTetrahedron : public GMSHSolidPolyhedron
        {
        public:
            GMSHTetrahedron( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id,
                      4, vertex_ids }
            {
            }

            geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
                const geode::uuid& block_uuid,
                const std::vector< geode::index_t >& v_ids ) override final
            {
                static const std::array< std::vector< geode::local_index_t >,
                    4 >
                    gmsh_tetrahedron_faces{ { { 0, 1, 2 }, { 0, 2, 3 },
                        { 1, 3, 2 }, { 0, 3, 1 } } };
                return builder.block_mesh_builder( block_uuid )
                    ->create_polyhedron( v_ids, gmsh_tetrahedron_faces );
            }
        };

        class GMSHHexahedron : public GMSHSolidPolyhedron
        {
        public:
            GMSHHexahedron( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id,
                      8, vertex_ids }
            {
            }

            geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
                const geode::uuid& block_uuid,
                const std::vector< geode::index_t >& v_ids ) override final
            {
                static const std::array< std::vector< geode::local_index_t >,
                    6 >
                    gmsh_hexahedron_faces{ { { 0, 1, 2, 3 }, { 7, 6, 5, 4 },
                        { 0, 3, 7, 4 }, { 1, 5, 6, 2 }, { 2, 6, 7, 3 },
                        { 0, 4, 5, 1 } } };
                return builder.block_mesh_builder( block_uuid )
                    ->create_polyhedron( v_ids, gmsh_hexahedron_faces );
            }
        };

        class GMSHPrism : public GMSHSolidPolyhedron
        {
        public:
            GMSHPrism( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id,
                      6, vertex_ids }
            {
            }

            geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
                const geode::uuid& block_uuid,
                const std::vector< geode::index_t >& v_ids ) override final
            {
                static const std::array< std::vector< geode::local_index_t >,
                    5 >
                    gmsh_prism_faces{ { { 0, 1, 2 }, { 5, 4, 3 },
                        { 0, 2, 5, 3 }, { 0, 3, 4, 1 }, { 1, 4, 5, 2 } } };
                return builder.block_mesh_builder( block_uuid )
                    ->create_polyhedron( v_ids, gmsh_prism_faces );
            }
        };

        class GMSHPyramid : public GMSHSolidPolyhedron
        {
        public:
            GMSHPyramid( geode::index_t physical_entity_id,
                geode::index_t elementary_entity_id,
                absl::Span< const std::string_view > vertex_ids )
                : GMSHSolidPolyhedron{ physical_entity_id, elementary_entity_id,
                      5, vertex_ids }
            {
            }

            geode::index_t create_gmsh_polyhedron( geode::BRepBuilder& builder,
                const geode::uuid& block_uuid,
                const std::vector< geode::index_t >& v_ids ) override final
            {
                static const std::array< std::vector< geode::local_index_t >,
                    5 >
                    gmsh_pyramid_faces{ { { 0, 3, 4 }, { 0, 4, 1 }, { 4, 3, 2 },
                        { 1, 4, 2 }, { 0, 1, 2, 3 } } };
                return builder.block_mesh_builder( block_uuid )
                    ->create_polyhedron( v_ids, gmsh_pyramid_faces );
            }
        };

        inline void initialize_gmsh_factory()
        {
            GMSHElementFactory::register_creator< GMSHPoint >( 15 );
            GMSHElementFactory::register_creator< GMSHEdge >( 1 );
            GMSHElementFactory::register_creator< GMSHTriangle >( 2 );
            GMSHElementFactory::register_creator< GMSHQuadrangle >( 3 );
            GMSHElementFactory::register_creator< GMSHTetrahedron >( 4 );
            GMSHElementFactory::register_creator< GMSHHexahedron >( 5 );
            GMSHElementFactory::register_creator< GMSHPrism >( 6 );
            GMSHElementFactory::register_creator< GMSHPyramid >( 7 );
        }
    } // namespace detail
} // namespace geode
