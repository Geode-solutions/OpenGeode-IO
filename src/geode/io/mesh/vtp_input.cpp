/*
 * Copyright (c) 2019 - 2020 Geode-solutions
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

#include <geode/io/mesh/private/vtp_input.h>

#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/core/polygonal_surface.h>

#include <geode/io/mesh/private/vtk_input.h>

namespace
{
    class VTPInputImpl
        : public geode::detail::VTKInputImpl< geode::PolygonalSurface3D,
              geode::PolygonalSurfaceBuilder3D >
    {
    public:
        VTPInputImpl( absl::string_view filename,
            geode::PolygonalSurface3D& polygonal_surface )
            : geode::detail::VTKInputImpl< geode::PolygonalSurface3D,
                geode::PolygonalSurfaceBuilder3D >(
                filename, polygonal_surface, "PolyData" )
        {
        }

    private:
        void read_vtk_cells( const pugi::xml_node& piece ) override
        {
            const auto nb_polygons = read_attribute( piece, "NumberOfPolys" );
            const auto polygon_offset =
                build_polygons( read_polygons( piece, nb_polygons ) );
            read_cell_data( piece.child( "CellData" ), polygon_offset );
        }

        absl::FixedArray< std::vector< geode::index_t > > read_polygons(
            const pugi::xml_node& piece, geode::index_t nb_polygons )
        {
            std::vector< int64_t > offsets_values;
            std::vector< int64_t > connectivity_values;
            for( const auto& data :
                piece.child( "Polys" ).children( "DataArray" ) )
            {
                if( match( data.attribute( "Name" ).value(), "offsets" ) )
                {
                    offsets_values = read_integer_data_array< int64_t >( data );
                    OPENGEODE_ASSERT( offsets_values.size() == nb_polygons,
                        "[VTKInput::read_polygons] Wrong number of offsets" );
                    geode_unused( nb_polygons );
                }
                else if( match( data.attribute( "Name" ).value(),
                             "connectivity" ) )
                {
                    connectivity_values =
                        read_integer_data_array< int64_t >( data );
                }
            }
            return get_cell_vertices( connectivity_values, offsets_values );
        }

        geode::index_t build_polygons(
            absl::Span< const std::vector< geode::index_t > > polygon_vertices )
        {
            absl::FixedArray< geode::index_t > new_polygons(
                polygon_vertices.size() );
            std::iota( new_polygons.begin(), new_polygons.end(),
                mesh().nb_polygons() );
            for( const auto& pv : polygon_vertices )
            {
                builder().create_polygon( pv );
            }
            builder().compute_polygon_adjacencies( new_polygons );
            return new_polygons[0];
        }

        void read_cell_data(
            const pugi::xml_node& point_data, geode::index_t offset )
        {
            for( const auto& data : point_data.children( "DataArray" ) )
            {
                const auto data_array_name = data.attribute( "Name" ).value();
                const auto data_array_type = data.attribute( "type" ).value();

                if( match( data_array_type, "Float64" )
                    || match( data_array_type, "Float32" ) )
                {
                    const auto attribute_values =
                        read_float_data_array< double >( data );
                    build_attribute( mesh().polygon_attribute_manager(),
                        data_array_name, attribute_values, offset );
                }
                else if( match( data_array_type, "Int64" )
                         || match( data_array_type, "Int32" )
                         || match( data_array_type, "UInt32" )
                         || match( data_array_type, "UInt64" ) )
                {
                    int64_t min_value;
                    absl::SimpleAtoi(
                        data.attribute( "RangeMin" ).value(), &min_value );
                    int64_t max_value;
                    absl::SimpleAtoi(
                        data.attribute( "RangeMax" ).value(), &max_value );
                    if( min_value >= 0
                        && max_value
                               < std::numeric_limits< geode::index_t >::max() )
                    {
                        const auto attribute_values =
                            read_integer_data_array< geode::index_t >( data );
                        build_attribute( mesh().polygon_attribute_manager(),
                            data_array_name, attribute_values, offset );
                    }
                    else
                    {
                        const auto attribute_values =
                            read_integer_data_array< long int >( data );
                        build_attribute( mesh().polygon_attribute_manager(),
                            data_array_name, attribute_values, offset );
                    }
                }
                else
                {
                    throw geode::OpenGeodeException(
                        "[VTKInput::read_point_data] Attribute of type ",
                        data_array_type, " is not supported" );
                }
            }
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void VTPInput::do_read()
        {
            VTPInputImpl impl{ filename(), polygonal_surface() };
            impl.read_file();
        }
    } // namespace detail
} // namespace geode
