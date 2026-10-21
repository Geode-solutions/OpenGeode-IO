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

#include <geode/io/mesh/detail/vtp_input.hpp>

#include <geode/mesh/builder/polygonal_surface_builder.hpp>
#include <geode/mesh/core/mesh_factory.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>

#include <geode/io/mesh/detail/vtk_mesh_input.hpp>

namespace
{
    class VTPInputImpl
        : public geode::detail::VTKMeshInputImpl< geode::PolygonalSurface3D >
    {
    public:
        VTPInputImpl( std::string_view filename, const geode::MeshImpl& impl )
            : geode::detail::VTKMeshInputImpl< geode::PolygonalSurface3D >(
                  filename, impl, "PolyData" )
        {
        }

    private:
        void read_vtk_cells( const pugi::xml_node& piece ) override
        {
            const auto nb_polygons = read_attribute( piece, "NumberOfPolys" );
            const auto cell_polygons =
                build_polygons( read_polygons( piece, nb_polygons ) );
            this->read_data( piece.child( "CellData" ),
                geode::detail::DataMapping{ cell_polygons },
                this->mesh().polygon_attribute_manager() );
        }

        geode::Percentage is_vtk_cells_loadable(
            const pugi::xml_node& piece ) const override
        {
            const auto nb_polygons = read_attribute( piece, "NumberOfPolys" );
            read_polygons( piece, nb_polygons );
            return geode::Percentage{ 1 };
        }

        geode::detail::VTKCells read_polygons(
            const pugi::xml_node& piece, geode::index_t nb_polygons ) const
        {
            std::vector< int64_t > offsets_values;
            std::vector< int64_t > connectivity_values;
            for( const auto& data :
                piece.child( "Polys" ).children( "DataArray" ) )
            {
                if( match( data.attribute( "Name" ).value(), "offsets" ) )
                {
                    offsets_values = read_data_array< int64_t >( data );
                }
                else if( match( data.attribute( "Name" ).value(),
                             "connectivity" ) )
                {
                    connectivity_values = read_data_array< int64_t >( data );
                }
            }
            geode::OpenGeodeIOMeshException::check_exception(
                offsets_values.size() == nb_polygons, nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[VTPInput::read_polygons] Wrong number of offsets: ",
                offsets_values.size(), " instead of ", nb_polygons );
            return get_cell_vertices(
                connectivity_values, std::move( offsets_values ) );
        }

        // Returns the polygon created for each cell, NO_ID if none
        std::vector< geode::index_t > build_polygons(
            const geode::detail::VTKCells& polygon_vertices )
        {
            std::vector< geode::index_t > cell_polygons(
                polygon_vertices.size(), geode::NO_ID );
            geode::index_t nb_invalid_polygons{ 0 };
            geode::index_t nb_degenerated_polygons{ 0 };
            for( const auto p : geode::Indices{ polygon_vertices } )
            {
                const auto pv = polygon_vertices[p];
                if( pv.size() < 3 )
                {
                    nb_degenerated_polygons++;
                    continue;
                }
                if( !are_cell_vertices_valid( pv ) )
                {
                    nb_invalid_polygons++;
                    continue;
                }
                cell_polygons[p] = builder().create_polygon( pv );
            }
            warn_invalid_cells( nb_invalid_polygons );
            if( nb_degenerated_polygons > 0 )
            {
                geode::Logger::warning( "[VTPInput] ", nb_degenerated_polygons,
                    " polygon(s) with less than 3 vertices have been "
                    "skipped" );
            }
            builder().compute_polygon_adjacencies();
            return cell_polygons;
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        std::unique_ptr< PolygonalSurface3D > VTPInput::read(
            const MeshImpl& impl )
        {
            VTPInputImpl reader{ filename(), impl };
            return reader.read_file();
        }

        Percentage VTPInput::is_loadable() const
        {
            VTPInputImpl reader{ filename(),
                MeshFactory::default_impl(
                    PolygonalSurface3D::type_name_static() ) };
            return reader.is_loadable();
        }
    } // namespace detail
} // namespace geode
