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

#include <geode/io/mesh/detail/common.h>

#include <fstream>

#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <pugixml.hpp>
#include <zlib-ng.h>

#include <geode/basic/attribute_manager.h>

#include <geode/geometry/bounding_box.h>
#include <geode/geometry/point.h>

#include <geode/io/mesh/private/base64.h>

namespace geode
{
    namespace detail
    {
        template < typename Mesh, typename MeshBuilder >
        class VTKOutputImpl
        {
        public:
            void write_file()
            {
                auto root = write_root_attributes();
                write_vtk_object( root );
            }

        protected:
            VTKOutputImpl( absl::string_view filename,
                const Mesh& polygonal_surface,
                const char* type )
                : filename_{ filename },
                  file_( filename.data() ),
                  mesh_( polygonal_surface ),
                  type_{ type }
            {
                OPENGEODE_EXCEPTION( file_.good(),
                    "[VTKOutput] Error while opening file: ", filename );
            }

            ~VTKOutputImpl()
            {
                const auto ok = document_.save_file( filename_.data() );
                OPENGEODE_EXCEPTION(
                    ok, "[VTKOutput] Error while writing file: ", filename_ );
            }

            const Mesh& mesh() const
            {
                return mesh_;
            }

        private:
            pugi::xml_node write_root_attributes()
            {
                auto root = document_.append_child( "VTKFile" );
                root.append_attribute( "type" ).set_value( "PolyData" );
                root.append_attribute( "version" ).set_value( "1.0" );
                root.append_attribute( "byte_order" )
                    .set_value( "LittleEndian" );
                root.append_attribute( "header_type" ).set_value( "UInt32" );
                root.append_attribute( "compressor" )
                    .set_value( "vtkZLibDataCompressor" );
                return root;
            }

            void write_vtk_object( pugi::xml_node& root )
            {
                auto object = root.append_child( type_ );
                auto piece = object.append_child( "Piece" );
                piece.append_attribute( "NumberOfPoints" )
                    .set_value( mesh_.nb_vertices() );
                piece.append_attribute( "NumberOfVerts" ).set_value( 0 );
                piece.append_attribute( "NumberOfLines" ).set_value( 0 );
                piece.append_attribute( "NumberOfStrips" ).set_value( 0 );
                append_number_elements( piece );

                write_vtk_vertex_attributes( piece );
                write_vtk_points( piece );
                write_vtk_cell_attributes( piece );
                write_vtk_cells( piece );
            }

            void write_vtk_points( pugi::xml_node& piece )
            {
                auto points = piece.append_child( "Points" );
                auto data_array = points.append_child( "DataArray" );
                data_array.append_attribute( "type" ).set_value( "Float32" );
                data_array.append_attribute( "Name" ).set_value( "Points" );
                data_array.append_attribute( "NumberOfComponents" )
                    .set_value( 3 );
                data_array.append_attribute( "format" ).set_value( "ascii" );
                const auto bbox = mesh().bounding_box();
                auto min = bbox.min().value( 0 );
                auto max = bbox.max().value( 0 );
                for( const auto d : Range{ 1, 3 } )
                {
                    min = std::min( min, bbox.min().value( d ) );
                    max = std::max( max, bbox.max().value( d ) );
                }
                data_array.append_attribute( "RangeMin" ).set_value( min );
                data_array.append_attribute( "RangeMax" ).set_value( max );
                std::vector< double > values;
                std::string vertices;
                for( const auto v : Range{ mesh().nb_vertices() } )
                {
                    vertices += mesh().point( v ).string();
                    vertices += " ";
                    values.emplace_back( mesh().point( v ).value( 0 ) );
                    values.emplace_back( mesh().point( v ).value( 1 ) );
                    values.emplace_back( mesh().point( v ).value( 2 ) );
                }
                data_array.text().set( vertices.c_str() );
            }

            std::string encode( const std::vector< double >& values ) {}

            void write_vtk_vertex_attributes( pugi::xml_node& piece )
            {
                auto point_data = piece.append_child( "PointData" );
                const auto names =
                    mesh().vertex_attribute_manager().attribute_names();
                for( const auto name : names )
                {
                    const auto attribute = mesh()
                                               .vertex_attribute_manager()
                                               .find_generic_attribute( name );
                    if( !attribute || !attribute->is_genericable() )
                    {
                        continue;
                    }
                    auto data_array = point_data.append_child( "DataArray" );
                    data_array.append_attribute( "type" ).set_value(
                        "Float64" );
                    data_array.append_attribute( "Name" ).set_value(
                        name.data() );
                    data_array.append_attribute( "format" )
                        .set_value( "ascii" );
                    auto min = attribute->generic_value( 0 );
                    auto max = attribute->generic_value( 0 );
                    std::string values;
                    for( const auto v : geode::Range{ mesh().nb_vertices() } )
                    {
                        const auto value = attribute->generic_value( v );
                        values += std::to_string( value );
                        values += " ";
                        min = std::min( min, value );
                        max = std::max( max, value );
                    }
                    data_array.append_attribute( "RangeMin" ).set_value( min );
                    data_array.append_attribute( "RangeMax" ).set_value( max );
                    data_array.text().set( values.c_str() );
                }
            }

            virtual void append_number_elements( pugi::xml_node& piece ) = 0;

            virtual void write_vtk_cells( pugi::xml_node& piece ) = 0;

            virtual void write_vtk_cell_attributes( pugi::xml_node& piece ) = 0;

        private:
            absl::string_view filename_;
            std::ofstream file_;
            const Mesh& mesh_;
            pugi::xml_document document_;
            const char* type_;
        };
    } // namespace detail
} // namespace geode
