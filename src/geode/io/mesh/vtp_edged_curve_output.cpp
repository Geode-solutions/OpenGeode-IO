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

#include <geode/io/mesh/private/vtp_edged_curve_output.h>

#include <geode/mesh/core/edged_curve.h>

#include <geode/io/mesh/private/vtk_mesh_output.h>

namespace
{
    template < geode::index_t dimension >
    class VTPCurveOutputImpl
        : public geode::detail::VTKMeshOutputImpl< geode::EdgedCurve,
              dimension >
    {
    public:
        VTPCurveOutputImpl( absl::string_view filename,
            const geode::EdgedCurve< dimension >& curve )
            : geode::detail::VTKMeshOutputImpl< geode::EdgedCurve, dimension >(
                filename, curve, "PolyData" )
        {
        }

    private:
        void append_number_elements( pugi::xml_node& piece ) override
        {
            piece.append_attribute( "NumberOfLines" )
                .set_value( this->mesh().nb_edges() );
        }

        pugi::xml_node write_vtk_cells( pugi::xml_node& piece ) override
        {
            auto lines = piece.append_child( "Lines" );
            auto connectivity = lines.append_child( "DataArray" );
            connectivity.append_attribute( "type" ).set_value( "Int64" );
            connectivity.append_attribute( "Name" ).set_value( "connectivity" );
            connectivity.append_attribute( "format" ).set_value( "ascii" );
            connectivity.append_attribute( "RangeMin" ).set_value( 0 );
            connectivity.append_attribute( "RangeMax" )
                .set_value( this->mesh().nb_vertices() - 1 );
            auto offsets = lines.append_child( "DataArray" );
            offsets.append_attribute( "type" ).set_value( "Int64" );
            offsets.append_attribute( "Name" ).set_value( "offsets" );
            offsets.append_attribute( "format" ).set_value( "ascii" );
            offsets.append_attribute( "RangeMin" ).set_value( 0 );
            offsets.append_attribute( "RangeMax" )
                .set_value( this->mesh().nb_vertices() );
            const auto nb_edges = this->mesh().nb_edges();
            std::string edge_connectivity;
            edge_connectivity.reserve( nb_edges * 2 );
            std::string edge_offsets;
            edge_offsets.reserve( nb_edges );
            geode::index_t vertex_count{ 0 };
            for( const auto e : geode::Range{ nb_edges } )
            {
                vertex_count += 2;
                absl::StrAppend( &edge_offsets, vertex_count, " " );
                for( const auto v : geode::LRange{ 2 } )
                {
                    absl::StrAppend( &edge_connectivity,
                        this->mesh().edge_vertex( { e, v } ), " " );
                }
            }
            connectivity.text().set( edge_connectivity.c_str() );
            offsets.text().set( edge_offsets.c_str() );
            return lines;
        }

        pugi::xml_node write_vtk_cell_attributes(
            pugi::xml_node& piece ) override
        {
            auto cell_data = piece.append_child( "CellData" );
            this->write_attributes(
                cell_data, this->mesh().edge_attribute_manager() );
            return cell_data;
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        void VTPEdgedCurveOutput< dimension >::write(
            const EdgedCurve< dimension >& curve ) const
        {
            VTPCurveOutputImpl< dimension > impl{ this->filename(), curve };
            impl.write_file();
        }

        template class VTPEdgedCurveOutput< 2 >;
        template class VTPEdgedCurveOutput< 3 >;
    } // namespace detail
} // namespace geode
