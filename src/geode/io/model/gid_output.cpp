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

#include <geode/io/model/internal/gid_output.hpp>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/constant_attribute.hpp>
#include <geode/basic/range.hpp>
#include <geode/basic/string.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/core/tetrahedral_solid.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/mixin/core/surface.hpp>
#include <geode/model/representation/core/brep.hpp>

namespace
{
    constexpr auto FRACSIMA_ATTRIBUTE_NAME = "material_number";
    constexpr geode::index_t NODE_OFFSET = 1;
    int get_material_number_value( const geode::Surface3D& surface )
    {
        auto attribute =
            surface.mesh()
                .polygon_attribute_manager()
                .find_or_create_attribute< geode::ConstantAttribute, int >(
                    FRACSIMA_ATTRIBUTE_NAME, 1, { false, true, true } );
        return attribute->value( 0 );
    }

    int get_material_number_value( const geode::Block3D& block )
    {
        auto attribute =
            block.mesh()
                .polyhedron_attribute_manager()
                .find_or_create_attribute< geode::ConstantAttribute, int >(
                    FRACSIMA_ATTRIBUTE_NAME, 1, { false, true, true } );
        return attribute->value( 0 );
    }

    class GIDOutputImpl
    {
    public:
        GIDOutputImpl( std::string_view filename, const geode::BRep& brep )
            : file_{ geode::to_string( filename ) }, brep_( brep )
        {
            OPENGEODE_EXCEPTION( file_.good(),
                "[GIDOutput] Error while opening file: ", filename );
        }

        void write_file()
        {
            write_header_block();
            write_tetrahedra_nodes();
            const auto nb_tet = write_tetrahedra();
            write_header_surfaces();
            write_triangles_nodes();
            write_triangles( nb_tet );
        }

    private:
        void write_header_block()
        {
            file_ << "MESH";
            file_ << geode::SPACE << "dimension" << geode::SPACE << 3;
            file_ << geode::SPACE << "ElemType" << geode::SPACE << "Tetrahedra"
                  << geode::SPACE << "Nnode" << geode::SPACE << 4 << geode::EOL;
        }

        void write_tetrahedra_nodes()
        {
            file_ << "Coordinates" << geode::EOL;
            for( const auto uv_index :
                geode::Range( brep_.nb_unique_vertices() ) )
            {
                for( const auto& cmv :
                    brep_.component_mesh_vertices( uv_index ) )
                {
                    if( cmv.component_id.type()
                        == geode::Block3D::component_type_static() )
                    {
                        file_ << uv_index + NODE_OFFSET << geode::SPACE;
                        file_ << brep_.block( cmv.component_id.id() )
                                     .mesh()
                                     .point( cmv.vertex )
                                     .string()
                              << geode::EOL;
                        break;
                    }
                }
            }
            file_ << "End Coordinates" << geode::EOL;
        }

        geode::index_t write_tetrahedra()
        {
            file_ << "Elements" << geode::EOL;
            geode::index_t nb_tet{ 0 };
            for( const auto& block : brep_.blocks() )
            {
                const auto material_number = get_material_number_value( block );
                for( const auto polyhedron_id :
                    geode::Range{ block.mesh().nb_polyhedra() } )
                {
                    file_ << polyhedron_id;
                    for( const auto vertex_lid : geode::LRange{ 4 } )
                    {
                        const auto tet_vertex = block.mesh().polyhedron_vertex(
                            { polyhedron_id, vertex_lid } );
                        const auto uid = brep_.unique_vertex(
                            { block.component_id(), tet_vertex } );
                        file_ << geode::SPACE << uid + NODE_OFFSET;
                    }
                    file_ << geode::SPACE << material_number << geode::EOL;
                }
                nb_tet += block.mesh().nb_polyhedra();
            }
            file_ << "End Elements" << geode::EOL;
            return nb_tet;
        }

        void write_header_surfaces()
        {
            file_ << "MESH";
            file_ << geode::SPACE << "dimension" << geode::SPACE << 3;
            file_ << geode::SPACE << "ElemType" << geode::SPACE << "Triangle"
                  << geode::SPACE << "Nnode" << geode::SPACE << 3 << geode::EOL;
        }

        bool is_unique_vertex_on_a_surface_outside_of_block(
            const geode::index_t uv_index )
        {
            bool is_vertex_on_surface = false;
            bool is_vertex_in_block = false;
            for( const auto& cmv : brep_.component_mesh_vertices( uv_index ) )
            {
                if( cmv.component_id.type()
                    == geode::Surface3D::component_type_static() )
                {
                    is_vertex_on_surface = true;
                }
                else if( cmv.component_id.type()
                         == geode::Block3D::component_type_static() )
                {
                    is_vertex_in_block = true;
                }
            }
            return is_vertex_on_surface && !is_vertex_in_block;
        }

        void write_triangles_nodes()
        {
            file_ << "Coordinates" << geode::EOL;
            for( const auto uv_index :
                geode::Range( brep_.nb_unique_vertices() ) )
            {
                if( !is_unique_vertex_on_a_surface_outside_of_block(
                        uv_index ) )
                {
                    continue;
                }
                for( const auto& cmv :
                    brep_.component_mesh_vertices( uv_index ) )
                {
                    file_ << uv_index + NODE_OFFSET << geode::SPACE;
                    file_ << brep_.surface( cmv.component_id.id() )
                                 .mesh()
                                 .point( cmv.vertex )
                                 .string()
                          << geode::EOL;
                    break;
                }
            }
            file_ << "End Coordinates" << geode::EOL;
        }

        void write_triangles( const geode::index_t nb_tet )
        {
            file_ << "Elements" << geode::EOL;
            for( const auto& surface : brep_.surfaces() )
            {
                const auto material_number =
                    get_material_number_value( surface );

                for( auto facet_id :
                    geode::Range{ surface.mesh().nb_polygons() } )
                {
                    file_ << nb_tet + facet_id;
                    for( const auto vertex_lid : geode::LRange{ 3 } )
                    {
                        const auto triangle_vertex =
                            surface.mesh().polygon_vertex(
                                { facet_id, vertex_lid } );
                        const auto uid = brep_.unique_vertex(
                            { surface.component_id(), triangle_vertex } );
                        file_ << geode::SPACE << uid + NODE_OFFSET;
                    }
                    file_ << geode::SPACE << material_number << geode::EOL;
                }
            }
            file_ << "End Elements" << geode::EOL;
        }

    private:
        std::ofstream file_;
        const geode::BRep& brep_;
    };
} // namespace

namespace geode::internal
{
    std::vector< std::string > GIDOutput::write( const BRep& brep ) const
    {
        GIDOutputImpl impl( filename(), brep );
        impl.write_file();
        return { to_string( filename() ) };
    }
    bool GIDOutput::is_saveable( const BRep& brep ) const
    {
        for( const auto& surface : brep.surfaces() )
        {
            if( surface.mesh().type_name()
                != TriangulatedSurface3D::type_name_static() )
            {
                return false;
            }
        }
        for( const auto& block : brep.blocks() )
        {
            if( block.mesh().type_name()
                != TetrahedralSolid3D::type_name_static() )
            {
                return false;
            }
        }
        return true;
    }
} // namespace geode::internal
