/*
 * Copyright (c) 2019 - 2023 Geode-solutions
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

#include <geode/io/model/private/msh_output.h>

#include <fstream>

#include <geode/geometry/bounding_box.h>
#include <geode/geometry/point.h>

#include <geode/mesh/core/edged_curve.h>
#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/solid_mesh.h>
#include <geode/mesh/core/surface_mesh.h>

#include <geode/model/mixin/core/block.h>
#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/mixin/core/surface.h>
#include <geode/model/representation/core/brep.h>

namespace
{
    static constexpr geode::index_t OFFSET_START{ 1 };
    static constexpr char EOL{ '\n' };
    static constexpr char SPACE{ ' ' };
    static constexpr geode::index_t DEFAULT_PHYSICAL_TAG{ 0 };

    struct GmshElementID
    {
        GmshElementID() = default;
        GmshElementID( geode::ComponentType gmsh_type, geode::index_t gmsh_id )
            : type( std::move( gmsh_type ) ), id( gmsh_id )
        {
        }

        bool operator==( const GmshElementID& other ) const
        {
            return type == other.type && id == other.id;
        }
        geode::ComponentType type{ "undefined" };
        geode::index_t id{ geode::NO_ID };
    };

    class MSHOutputImpl
    {
    public:
        MSHOutputImpl( absl::string_view filename, const geode::BRep& brep )
            : file_{ geode::to_string( filename ) }, brep_( brep )
        {
            OPENGEODE_EXCEPTION( file_.good(),
                "[MSHOutput] Error while opening file: ", filename );
        }

        void write_file()
        {
            write_header();
            write_entities();
            write_nodes();
            write_elements();
        }

    private:
        enum struct GmshElement
        {
            POINT,
            EDGE,
            TRIANGLE,
            QUADRANGLE,
            TETRAHEDRON,
            HEXAHEDRON,
            PRISM,
            PYRAMID
        };

        void write_header()
        {
            file_ << "$MeshFormat" << EOL;
            file_ << 4.1 << SPACE << 0 << SPACE << 8 << EOL;
            file_ << "$EndMeshFormat" << EOL;
        }

        void write_entities()
        {
            file_ << "$Entities" << EOL;
            file_ << brep_.nb_corners() << SPACE << brep_.nb_lines() << SPACE
                  << brep_.nb_surfaces() << SPACE << brep_.nb_blocks() << EOL;
            write_corners();
            write_lines();
            write_surfaces();
            write_blocks();
            file_ << "$EndEntities" << EOL;
        }

        template < typename Component >
        void write_component(
            const Component& component, geode::index_t gmsh_id )
        {
            const auto bbox = component.mesh().bounding_box();
            file_ << gmsh_id << SPACE << bbox.min().string() << SPACE
                  << bbox.max().string() << SPACE << DEFAULT_PHYSICAL_TAG
                  << SPACE;
            file_ << brep_.nb_boundaries( component.id() );
            for( const auto& boundary : brep_.boundaries( component ) )
            {
                file_ << SPACE << uuid2gmsh_[boundary.id()].id;
            }
            file_ << EOL;
            uuid2gmsh_[component.id()] =
                GmshElementID{ component.component_type(), gmsh_id };
        }

        void write_corners()
        {
            geode::index_t count{ 1 };
            for( const auto& corner : brep_.corners() )
            {
                const auto& point = corner.mesh().point( 0 );
                file_ << count << SPACE << point.string() << SPACE
                      << DEFAULT_PHYSICAL_TAG << EOL;
                uuid2gmsh_[corner.id()] =
                    GmshElementID{ corner.component_type(), count++ };
            }
        }

        void write_lines()
        {
            geode::index_t count{ 1 };
            for( const auto& line : brep_.lines() )
            {
                write_component( line, count++ );
            }
        }

        void write_surfaces()
        {
            geode::index_t count{ 1 };
            for( const auto& surface : brep_.surfaces() )
            {
                const auto bbox = surface.mesh().bounding_box();
                file_ << count << SPACE << bbox.min().string() << SPACE
                      << bbox.max().string() << SPACE << DEFAULT_PHYSICAL_TAG
                      << SPACE;
                file_ << brep_.nb_boundaries( surface.id() )
                             + 2 * brep_.nb_internal_lines( surface );
                for( const auto& boundary : brep_.boundaries( surface ) )
                {
                    file_ << SPACE << uuid2gmsh_[boundary.id()].id;
                }
                for( const auto& internal : brep_.internal_lines( surface ) )
                {
                    file_ << SPACE << uuid2gmsh_[internal.id()].id << SPACE
                          << "-" << uuid2gmsh_[internal.id()].id;
                }
                file_ << EOL;
                uuid2gmsh_[surface.id()] =
                    GmshElementID{ surface.component_type(), count };
                count++;
            }
        }

        void write_blocks()
        {
            geode::index_t count{ 1 };
            for( const auto& block : brep_.blocks() )
            {
                const auto bbox = block.mesh().bounding_box();
                file_ << count << SPACE << bbox.min().string() << SPACE
                      << bbox.max().string() << SPACE << DEFAULT_PHYSICAL_TAG
                      << SPACE;
                file_ << brep_.nb_boundaries( block.id() )
                             + 2 * brep_.nb_internal_surfaces( block );
                for( const auto& boundary : brep_.boundaries( block ) )
                {
                    file_ << SPACE << uuid2gmsh_[boundary.id()].id;
                }
                for( const auto& internal : brep_.internal_surfaces( block ) )
                {
                    file_ << SPACE << uuid2gmsh_[internal.id()].id << SPACE
                          << "-" << uuid2gmsh_[internal.id()].id;
                }
                file_ << EOL;
                uuid2gmsh_[block.id()] =
                    GmshElementID{ block.component_type(), count };
                count++;
            }
        }

        template < typename Component >
        void write_component_nodes( const Component& component,
            absl::FixedArray< bool >& node_exported )
        {
            const auto nb_component_vertices = component.mesh().nb_vertices();
            std::vector< std::pair< geode::index_t, geode::index_t > >
                nodes_to_export;
            nodes_to_export.reserve( nb_component_vertices );
            for( const auto v : geode::Range{ nb_component_vertices } )
            {
                const auto uid =
                    brep_.unique_vertex( { component.component_id(), v } );
                if( !node_exported[uid] )
                {
                    nodes_to_export.push_back( { v, uid } );
                }
            }
            file_ << component_type2dimension[component.component_type()]
                  << SPACE << uuid2gmsh_[component.id()].id << SPACE << 0
                  << SPACE << nodes_to_export.size() << EOL;
            for( const auto& vertex_pair : nodes_to_export )
            {
                file_ << OFFSET_START + vertex_pair.second << EOL;
                node_exported[vertex_pair.second] = true;
            }
            for( const auto& vertex_pair : nodes_to_export )
            {
                file_ << component.mesh().point( vertex_pair.first ).string()
                      << EOL;
            }
        }

        void write_nodes()
        {
            file_ << "$Nodes" << EOL;
            file_ << brep_.nb_corners() + brep_.nb_lines() + brep_.nb_surfaces()
                         + brep_.nb_blocks()
                  << SPACE << brep_.nb_unique_vertices() << SPACE
                  << OFFSET_START << SPACE << brep_.nb_unique_vertices() << EOL;
            absl::FixedArray< bool > node_exported(
                brep_.nb_unique_vertices(), false );

            for( const auto& corner : brep_.corners() )
            {
                write_component_nodes( corner, node_exported );
            }
            for( const auto& line : brep_.lines() )
            {
                write_component_nodes( line, node_exported );
            }
            for( const auto& surface : brep_.surfaces() )
            {
                write_component_nodes( surface, node_exported );
            }
            for( const auto& block : brep_.blocks() )
            {
                write_component_nodes( block, node_exported );
            }
            file_ << "$EndNodes" << EOL;
        }

        geode::index_t count_elements()
        {
            geode::index_t nb_total_elements{ 0 };
            for( const auto& corner : brep_.corners() )
            {
                nb_total_elements += corner.mesh().nb_vertices();
            }
            for( const auto& line : brep_.lines() )
            {
                nb_total_elements += line.mesh().nb_edges();
            }
            for( const auto& surface : brep_.surfaces() )
            {
                nb_total_elements += surface.mesh().nb_polygons();
            }
            for( const auto& block : brep_.blocks() )
            {
                nb_total_elements += block.mesh().nb_polyhedra();
            }
            return nb_total_elements;
        }

        geode::index_t write_corner_elements( geode::index_t first )
        {
            auto cur = first;
            for( const auto& corner : brep_.corners() )
            {
                file_ << component_type2dimension[corner.component_type()]
                      << SPACE << uuid2gmsh_[corner.id()].id << SPACE
                      << element2type[GmshElement::POINT] << SPACE
                      << corner.mesh().nb_vertices() << EOL;
                for( const auto v :
                    geode::Range{ corner.mesh().nb_vertices() } )
                {
                    const auto uid =
                        brep_.unique_vertex( { corner.component_id(), v } );
                    file_ << cur++ << SPACE << OFFSET_START + uid << EOL;
                }
            }
            return cur;
        }

        geode::index_t write_line_elements( geode::index_t first )
        {
            auto cur = first;
            for( const auto& line : brep_.lines() )
            {
                file_ << component_type2dimension[line.component_type()]
                      << SPACE << uuid2gmsh_[line.id()].id << SPACE
                      << element2type[GmshElement::EDGE] << SPACE
                      << line.mesh().nb_edges() << EOL;
                for( const auto e : geode::Range{ line.mesh().nb_edges() } )
                {
                    file_ << cur++;
                    for( const auto v : geode::LRange{ 2 } )
                    {
                        const auto edge_vertex =
                            line.mesh().edge_vertex( { e, v } );
                        const auto uid = brep_.unique_vertex(
                            { line.component_id(), edge_vertex } );
                        file_ << SPACE << OFFSET_START + uid;
                    }
                    file_ << EOL;
                }
            }
            return cur;
        }

        // TODO : more gmsh block if multi element//
        geode::index_t write_surface_elements( geode::index_t first )
        {
            auto cur = first;
            for( const auto& surface : brep_.surfaces() )
            {
                file_ << component_type2dimension[surface.component_type()]
                      << SPACE << uuid2gmsh_[surface.id()].id << SPACE
                      << element2type[GmshElement::TRIANGLE] << SPACE
                      << surface.mesh().nb_polygons() << EOL;
                for( const auto p :
                    geode::Range{ surface.mesh().nb_polygons() } )
                {
                    file_ << cur++;
                    for( const auto v : geode::LRange{ 3 } )
                    {
                        const auto facet_vertex =
                            surface.mesh().polygon_vertex( { p, v } );
                        const auto uid = brep_.unique_vertex(
                            { surface.component_id(), facet_vertex } );
                        file_ << SPACE << OFFSET_START + uid;
                    }
                    file_ << EOL;
                }
            }
            return cur;
        }

        // TODO : more gmsh block if multi element//
        geode::index_t write_block_elements( geode::index_t first )
        {
            auto cur = first;
            for( const auto& block : brep_.blocks() )
            {
                file_ << component_type2dimension[block.component_type()]
                      << SPACE << uuid2gmsh_[block.id()].id << SPACE
                      << element2type[GmshElement::TETRAHEDRON] << SPACE
                      << block.mesh().nb_polyhedra() << EOL;
                for( const auto p :
                    geode::Range{ block.mesh().nb_polyhedra() } )
                {
                    file_ << cur++;
                    for( const auto v : geode::LRange{ 4 } )
                    {
                        const auto facet_vertex =
                            block.mesh().polyhedron_vertex( { p, v } );
                        const auto uid = brep_.unique_vertex(
                            { block.component_id(), facet_vertex } );
                        file_ << SPACE << OFFSET_START + uid;
                    }
                    file_ << EOL;
                }
            }
            return cur;
        }

        void write_elements()
        {
            file_ << "$Elements" << EOL;
            // TODO : more gmsh block if multi element//
            file_ << brep_.nb_corners() + brep_.nb_lines() + brep_.nb_surfaces()
                         + brep_.nb_blocks()
                  << SPACE;
            const auto nb_total_elements = count_elements();
            file_ << nb_total_elements << SPACE << OFFSET_START << SPACE
                  << nb_total_elements << EOL;

            geode::index_t element_count{ OFFSET_START };
            element_count = write_corner_elements( element_count );
            element_count = write_line_elements( element_count );
            element_count = write_surface_elements( element_count );
            element_count = write_block_elements( element_count );
            file_ << "$EndElements" << EOL;
        }

    private:
        std::ofstream file_;
        const geode::BRep& brep_;
        bool binary_{ true };
        double version_{ 4 };
        absl::flat_hash_map< geode::uuid, GmshElementID > uuid2gmsh_;
        absl::flat_hash_map< geode::ComponentType, geode::index_t >
            component_type2dimension = {
                { geode::Corner3D::component_type_static(), 0 },
                { geode::Line3D::component_type_static(), 1 },
                { geode::Surface3D::component_type_static(), 2 },
                { geode::Block3D::component_type_static(), 3 }
            };
        absl::flat_hash_map< GmshElement, geode::index_t > element2type = {
            { GmshElement::POINT, 15 }, { GmshElement::EDGE, 1 },
            { GmshElement::TRIANGLE, 2 }, { GmshElement::QUADRANGLE, 3 },
            { GmshElement::TETRAHEDRON, 4 }, { GmshElement::HEXAHEDRON, 5 },
            { GmshElement::PRISM, 6 }, { GmshElement::PYRAMID, 7 }
        };
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void MSHOutput::write( const BRep& brep ) const
        {
            MSHOutputImpl impl( filename(), brep );
            impl.write_file();
        }
    } // namespace detail
} // namespace geode
