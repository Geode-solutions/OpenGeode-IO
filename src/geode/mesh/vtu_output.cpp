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

#include <geode/basic/attribute.h>
#include <geode/basic/attribute_manager.h>
#include <geode/basic/range.h>

#include <geode/geometry/point.h>

#include <geode/mesh/core/polyhedral_solid.h>

namespace
{
    class VTUOutputImpl
    {
    public:
        VTUOutputImpl( const geode::PolyhedralSolid3D& solid,
            const std::string& filename,
            const std::unordered_set< std::string >& exported_types )
            : solid_( solid ),
              filename_( filename ),
              exported_types_( exported_types )
        {
        }

        void save_solid()
        {
            prepare_document();
            save_vertex_attributes();
            save_cell_attributes();
            save_vertices();
            save_cells();
            save_file();
        }

    private:
        void prepare_document()
        {
            auto vtkfile = document_.append_child( "VTKFile" );
            vtkfile.append_attribute( "type" ).set_value( "UnstructuredGrid" );
            vtkfile.append_attribute( "version" ).set_value( "0.1" );
            auto ugrid = vtkfile.append_child( "UnstructuredGrid" );
            node_ = ugrid.append_child( "Piece" );
            node_.append_attribute( "NumberOfPoints" )
                .set_value( solid_.nb_vertices() );
            node_.append_attribute( "NumberOfCells" )
                .set_value( solid_.nb_polyhedra() );
        }

        void save_vertex_attributes()
        {
            const auto& vertex_attribute_manager =
                solid_.vertex_attribute_manager();
            const auto& va_names = vertex_attribute_manager.attribute_names();
            auto point_data = node_.append_child( "PointData" );
            point_data.append_attribute( "Scalars" ).set_value( "scalars" );
            for( const auto& name : va_names )
            {
                if( this->exported_types_.find(
                        vertex_attribute_manager.attribute_type( name ) )
                    != this->exported_types_.end() )
                {
                    auto attribute = point_data.append_child( "DataArray" );
                    attribute.append_attribute( "type" ).set_value( "Float32" );
                    attribute.append_attribute( "Name" ).set_value(
                        name.c_str() );
                    attribute.append_attribute( "Format" ).set_value( "ascii" );
                    attribute.text().set(
                        print_attribute( vertex_attribute_manager, name )
                            .c_str() );
                }
            }
        }

        void save_cell_attributes()
        {
            const auto& cell_attribute_manager =
                solid_.polyhedron_attribute_manager();
            const auto& names = cell_attribute_manager.attribute_names();
            auto cell_data = node_.append_child( "CellData" );
            cell_data.append_attribute( "Scalars" ).set_value( "scalars" );
            for( const auto& name : names )
            {
                if( this->exported_types_.find(
                        cell_attribute_manager.attribute_type( name ) )
                    != this->exported_types_.end() )
                {
                    auto attribute = cell_data.append_child( "DataArray" );
                    attribute.append_attribute( "type" ).set_value( "Float32" );
                    attribute.append_attribute( "Name" ).set_value(
                        name.c_str() );
                    attribute.append_attribute( "Format" ).set_value( "ascii" );
                    attribute.text().set(
                        print_attribute( cell_attribute_manager, name )
                            .c_str() );
                }
            }
        }

        void save_vertices()
        {
            auto points = node_.append_child( "Points" );
            auto pt_data_array = points.append_child( "DataArray" );
            pt_data_array.append_attribute( "type" ).set_value( "Float32" );
            pt_data_array.append_attribute( "NumberOfComponents" )
                .set_value( "3" );
            pt_data_array.append_attribute( "Format" ).set_value( "ascii" );
            pt_data_array.text().set( print_point_coords().c_str() );
        }

        void save_cells()
        {
            auto cells = node_.append_child( "Cells" );
            auto connectivity = cells.append_child( "DataArray" );
            connectivity.append_attribute( "Name" ).set_value( "connectivity" );
            connectivity.append_attribute( "type" ).set_value( "Int32" );
            connectivity.append_attribute( "Format" ).set_value( "ascii" );
            connectivity.text().set( print_cell_vertices().c_str() );

            auto offset = cells.append_child( "DataArray" );
            offset.append_attribute( "Name" ).set_value( "offsets" );
            offset.append_attribute( "type" ).set_value( "Int32" );
            offset.append_attribute( "Format" ).set_value( "ascii" );
            offset.text().set( print_cell_offsets().c_str() );

            auto types = cells.append_child( "DataArray" );
            types.append_attribute( "Name" ).set_value( "types" );
            types.append_attribute( "type" ).set_value( "Int32" );
            types.append_attribute( "Format" ).set_value( "ascii" );
            types.text().set( print_cell_types().c_str() );

            auto faces = cells.append_child( "DataArray" );
            faces.append_attribute( "Name" ).set_value( "faces" );
            faces.append_attribute( "type" ).set_value( "Int32" );
            faces.append_attribute( "Format" ).set_value( "ascii" );
            faces.text().set( print_cell_faces().c_str() );

            auto face_offsets = cells.append_child( "DataArray" );
            face_offsets.append_attribute( "Name" ).set_value( "faceoffsets" );
            face_offsets.append_attribute( "type" ).set_value( "Int32" );
            face_offsets.append_attribute( "Format" ).set_value( "ascii" );
            face_offsets.text().set( print_cell_face_offsets().c_str() );
        }

        void save_file()
        {
            document_.save_file( filename_.c_str(), "\t", pugi::format_default,
                pugi::encoding_auto );
        }

        std::string print_point_coords()
        {
            std::string result;
            for( const auto v : geode::Range{ solid_.nb_vertices() } )
            {
                result += "\n";
                for( const auto c : geode::Range{ 3 } )
                {
                    result += std::to_string( solid_.point( v ).value( c ) );
                    result += " ";
                }
            }
            result += "\n";
            return result;
        }

        std::string print_cell_vertices()
        {
            std::string result;
            for( const auto p : geode::Range{ solid_.nb_polyhedra() } )
            {
                result += "\n";
                for( const auto v :
                    geode::Range{ solid_.nb_polyhedron_vertices( p ) } )
                {
                    result +=
                        std::to_string( solid_.polyhedron_vertex( { p, v } ) );
                    result += " ";
                }
            }
            result += "\n";
            return result;
        }

        std::string print_cell_offsets()
        {
            std::string result;
            geode::index_t counter{ 0 };
            result += "\n";
            for( const auto p : geode::Range{ solid_.nb_polyhedra() } )
            {
                counter += solid_.nb_polyhedron_vertices( p );
                result += std::to_string( counter );
                result += " ";
            }
            result += "\n";
            return result;
        }

        std::string print_cell_types()
        {
            std::string result;
            result += "\n";
            for( const auto p : geode::Range{ solid_.nb_polyhedra() } )
            {
                geode_unused( p );
                result += "42 ";
            }
            result += "\n";
            return result;
        }

        std::string print_cell_faces()
        {
            std::string result;
            for( const auto p : geode::Range{ solid_.nb_polyhedra() } )
            {
                result += "\n";
                result += std::to_string( solid_.nb_polyhedron_facets( p ) );
                for( const auto f :
                    geode::Range{ solid_.nb_polyhedron_facets( p ) } )
                {
                    result += "\n";
                    const auto vertices = solid_.facet_vertices(
                        solid_.polyhedron_facet( { p, f } ) );
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

        std::string print_cell_face_offsets()
        {
            std::string result;
            geode::index_t counter{ 0 };
            result += "\n";
            for( const auto p : geode::Range{ solid_.nb_polyhedra() } )
            {
                counter++;
                for( const auto f :
                    geode::Range{ solid_.nb_polyhedron_facets( p ) } )
                {
                    counter++;
                    counter += solid_.nb_polyhedron_facet_vertices( { p, f } );
                }
                result += std::to_string( counter );
                result += " ";
            }
            result += "\n";
            return result;
        }

        std::string print_attribute( const geode::AttributeManager& manager,
            const std::string& attribute_name )
        {
            const auto attribute =
                manager.find_generic_attribute( attribute_name );
            std::string result;
            result += "\n";
            for( const auto p : geode::Range{ manager.nb_elements() } )
            {
                result += std::to_string( attribute->generic_value( p ) );
                result += " ";
            }
            result += "\n";
            return result;
        }

    private:
        const geode::PolyhedralSolid3D& solid_;
        const std::string& filename_;
        const std::unordered_set< std::string >& exported_types_;
        pugi::xml_document document_;
        pugi::xml_node node_;
    };
} // namespace

namespace geode
{
    void VTUOutput::write() const
    {
        VTUOutputImpl impl( polyhedral_solid(), filename(), exported_types_ );
        impl.save_solid();
    }
} // namespace geode
