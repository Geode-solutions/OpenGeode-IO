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

#include <geode/mesh/detail/vtu_output.h>

#include <pugixml.hpp>

#include <geode/basic/range.h>

#include <geode/geometry/point.h>

#include <geode/mesh/core/polyhedral_solid.h>

namespace
{
    // void write_point_data() {}
    // void write_cell_data() {}
    // void write_points() {}
    // void write_cells() {}

    std::string print_point_coords( const geode::PolyhedralSolid3D& solid )
    {
        std::string result;
        for( const auto v : geode::Range{ solid.nb_vertices() } )
        {
            result += "\n";
            for( const auto c : geode::Range{ 3 } )
            {
                result += std::to_string( solid.point( v ).value( c ) );
                result += " ";
            }
        }
        result += "\n";
        return result;
    }

    std::string print_cell_vertices( const geode::PolyhedralSolid3D& solid )
    {
        std::string result;
        for( const auto p : geode::Range{ solid.nb_polyhedra() } )
        {
            result += "\n";
            for( const auto v :
                geode::Range{ solid.nb_polyhedron_vertices( p ) } )
            {
                result += std::to_string( solid.polyhedron_vertex( { p, v } ) );
                result += " ";
            }
        }
        result += "\n";
        return result;
    }

    std::string print_cell_offsets( const geode::PolyhedralSolid3D& solid )
    {
        std::string result;
        geode::index_t counter{ 0 };
        result += "\n";
        for( const auto p : geode::Range{ solid.nb_polyhedra() } )
        {
            counter += solid.nb_polyhedron_vertices( p );
            result += std::to_string( counter );
            result += " ";
        }
        result += "\n";
        return result;
    }

    std::string print_cell_types( const geode::PolyhedralSolid3D& solid )
    {
        std::string result;
        result += "\n";
        for( const auto p : geode::Range{ solid.nb_polyhedra() } )
        {
            geode_unused( p );
            result += "42 ";
        }
        result += "\n";
        return result;
    }

    std::string print_cell_faces( const geode::PolyhedralSolid3D& solid )
    {
        std::string result;
        geode::index_t counter{ 0 };
        result += "\n";
        for( const auto p : geode::Range{ solid.nb_polyhedra() } )
        {
            counter += solid.nb_polyhedron_facets( p );
        }
        result += std::to_string( counter );
        for( const auto p : geode::Range{ solid.nb_polyhedra() } )
        {
            for( const auto f :
                geode::Range{ solid.nb_polyhedron_facets( p ) } )
            {
                result += "\n";
                const auto vertices =
                    solid.facet_vertices( solid.polyhedron_facet( { p, f } ) );
                result += std::to_string( vertices.size() );
                for( const auto v : vertices )
                {
                    result += " ";
                    result += std::to_string( v );
                }
            }
        }
        result += "\n";
        return result;
    }

    std::string print_cell_face_offsets( const geode::PolyhedralSolid3D& solid )
    {
        std::string result;
        geode::index_t counter{ 0 };
        result += "\n";
        for( const auto p : geode::Range{ solid.nb_polyhedra() } )
        {
            for( const auto f :
                geode::Range{ solid.nb_polyhedron_facets( p ) } )
            {
                counter++;
                counter += solid.nb_polyhedron_facet_vertices( { p, f } );
            }
            result += std::to_string( counter );
            result += " ";
        }
        result += "\n";
        return result;
    }
} // namespace

namespace geode
{
    void VTUOutput::write() const
    {
        // std::ofstream writer{ filename() };
        pugi::xml_document doc;
        auto vtkfile = doc.append_child( "VTKFile" );
        vtkfile.append_attribute( "type" ).set_value( "UnstructuredGrid" );
        vtkfile.append_attribute( "version" ).set_value( "0.1" );
        auto ugrid = vtkfile.append_child( "UnstructuredGrid" );
        auto piece = ugrid.append_child( "Piece" );
        piece.append_attribute( "NumberOfPoints" )
            .set_value( polyhedral_solid().nb_vertices() );
        piece.append_attribute( "NumberOfCells" )
            .set_value( polyhedral_solid().nb_polyhedra() );

        // write_header( writer, polyhedral_solid() );
        // write_point_data();
        // write_cell_data();
        // write_points();
        auto points = piece.append_child( "Points" );
        auto pt_data_array = points.append_child( "DataArray" );
        pt_data_array.append_attribute( "type" ).set_value( "Float32" );
        pt_data_array.append_attribute( "NumberOfComponents" ).set_value( "3" );
        pt_data_array.append_attribute( "Format" ).set_value( "ascii" );
        pt_data_array.text().set(
            print_point_coords( polyhedral_solid() ).c_str() );
        // write_cells();
        auto cells = piece.append_child( "Cells" );
        auto connectivity = cells.append_child( "DataArray" );
        connectivity.append_attribute( "Name" ).set_value( "connectivity" );
        connectivity.append_attribute( "type" ).set_value( "Int32" );
        connectivity.append_attribute( "Format" ).set_value( "ascii" );
        connectivity.text().set(
            print_cell_vertices( polyhedral_solid() ).c_str() );

        auto offset = cells.append_child( "DataArray" );
        offset.append_attribute( "Name" ).set_value( "offsets" );
        offset.append_attribute( "type" ).set_value( "Int32" );
        offset.append_attribute( "Format" ).set_value( "ascii" );
        offset.text().set( print_cell_offsets( polyhedral_solid() ).c_str() );

        auto types = cells.append_child( "DataArray" );
        types.append_attribute( "Name" ).set_value( "types" );
        types.append_attribute( "type" ).set_value( "Int32" );
        types.append_attribute( "Format" ).set_value( "ascii" );
        types.text().set( print_cell_types( polyhedral_solid() ).c_str() );

        auto faces = cells.append_child( "DataArray" );
        faces.append_attribute( "Name" ).set_value( "faces" );
        faces.append_attribute( "type" ).set_value( "Int32" );
        faces.append_attribute( "Format" ).set_value( "ascii" );
        faces.text().set( print_cell_faces( polyhedral_solid() ).c_str() );

        auto face_offsets = cells.append_child( "DataArray" );
        face_offsets.append_attribute( "Name" ).set_value( "faceoffsets" );
        face_offsets.append_attribute( "type" ).set_value( "Int32" );
        face_offsets.append_attribute( "Format" ).set_value( "ascii" );
        face_offsets.text().set(
            print_cell_face_offsets( polyhedral_solid() ).c_str() );

        // write_footer( writer );
        doc.save_file( filename().c_str(), "\t", pugi::format_default,
            pugi::encoding_auto );
    }
} // namespace geode
