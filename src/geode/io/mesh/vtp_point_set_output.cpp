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

#include <geode/io/mesh/private/vtp_point_set_output.h>

#include <geode/mesh/core/point_set.h>

#include <geode/io/mesh/private/vtk_mesh_output.h>

namespace
{
    template < geode::index_t dimension >
    class VTPPointOutputImpl
        : public geode::detail::VTKMeshOutputImpl< geode::PointSet, dimension >
    {
    public:
        VTPPointOutputImpl( absl::string_view filename,
            const geode::PointSet< dimension >& point_set )
            : geode::detail::VTKMeshOutputImpl< geode::PointSet, dimension >(
                filename, point_set, "PolyData" )
        {
        }

    private:
        void append_number_elements( pugi::xml_node& piece ) override
        {
            piece.append_attribute( "NumberOfVerts" )
                .set_value( this->mesh().nb_vertices() );
        }

        pugi::xml_node write_vtk_cells( pugi::xml_node& piece ) override
        {
            auto verts = piece.append_child( "Verts" );
            auto connectivity = verts.append_child( "DataArray" );
            connectivity.append_attribute( "type" ).set_value( "Int64" );
            connectivity.append_attribute( "Name" ).set_value( "connectivity" );
            connectivity.append_attribute( "format" ).set_value( "ascii" );
            connectivity.append_attribute( "RangeMin" ).set_value( 0 );
            connectivity.append_attribute( "RangeMax" )
                .set_value( this->mesh().nb_vertices() - 1 );
            auto offsets = verts.append_child( "DataArray" );
            offsets.append_attribute( "type" ).set_value( "Int64" );
            offsets.append_attribute( "Name" ).set_value( "offsets" );
            offsets.append_attribute( "format" ).set_value( "ascii" );
            offsets.append_attribute( "RangeMin" ).set_value( 0 );
            offsets.append_attribute( "RangeMax" )
                .set_value( this->mesh().nb_vertices() );
            const auto nb_vertices = this->mesh().nb_vertices();
            std::string vertex_connectivity;
            vertex_connectivity.reserve( nb_vertices );
            std::string vertex_offsets;
            vertex_offsets.reserve( nb_vertices );
            for( const auto v : geode::Range{ nb_vertices } )
            {
                absl::StrAppend( &vertex_offsets, v + 1, " " );
                absl::StrAppend( &vertex_connectivity, v, " " );
            }
            connectivity.text().set( vertex_connectivity.c_str() );
            offsets.text().set( vertex_offsets.c_str() );
            return verts;
        }

        pugi::xml_node write_vtk_cell_attributes(
            pugi::xml_node& /*unsued*/ ) override
        {
            return {};
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        void VTPPointSetOutput< dimension >::write(
            const PointSet< dimension >& point_set ) const
        {
            VTPPointOutputImpl< dimension > impl{ this->filename(), point_set };
            impl.write_file();
        }

        template class VTPPointSetOutput< 2 >;
        template class VTPPointSetOutput< 3 >;
    } // namespace detail
} // namespace geode
