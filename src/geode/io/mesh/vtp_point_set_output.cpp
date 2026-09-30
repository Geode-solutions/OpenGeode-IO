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

#include <geode/io/mesh/detail/vtp_point_set_output.hpp>

#include <string>

#include <geode/mesh/core/point_set.hpp>

#include <geode/io/mesh/detail/vtk_mesh_output.hpp>

namespace
{
    template < geode::index_t dimension >
    class VTPPointOutputImpl
        : public geode::detail::VTKMeshOutputImpl< geode::PointSet, dimension >
    {
    public:
        VTPPointOutputImpl( std::string_view filename,
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
            const auto nb_vertices = this->mesh().nb_vertices();
            std::vector< int64_t > vertex_connectivity( nb_vertices );
            absl::c_iota( vertex_connectivity, 0 );
            std::vector< int64_t > vertex_offsets( nb_vertices );
            absl::c_iota( vertex_offsets, 1 );
            this->template write_data_array< int64_t >(
                verts, "connectivity", vertex_connectivity );
            this->template write_data_array< int64_t >(
                verts, "offsets", vertex_offsets );
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
        std::vector< std::string > VTPPointSetOutput< dimension >::write(
            const PointSet< dimension >& point_set ) const
        {
            VTPPointOutputImpl< dimension > impl{ this->filename(), point_set };
            impl.write_file();
            return { to_string( this->filename() ) };
        }

        template class VTPPointSetOutput< 2 >;
        template class VTPPointSetOutput< 3 >;
    } // namespace detail
} // namespace geode
