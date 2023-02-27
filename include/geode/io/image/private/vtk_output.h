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

#pragma once

#include <geode/io/mesh/common.h>

#include <fstream>

#include <pugixml.hpp>

#include <geode/basic/attribute_manager.h>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class VTKOutputImpl
        {
        public:
            void write_file()
            {
                auto root = write_root_attributes();
                write_vtk_object( root );
                document_.save( file_ );
            }

        protected:
            VTKOutputImpl(
                absl::string_view filename, const Mesh& mesh, const char* type )
                : filename_{ filename },
                  file_{ to_string( filename ) },
                  mesh_( mesh ),
                  type_{ type }
            {
                OPENGEODE_EXCEPTION( file_.good(),
                    "[VTKOutput] Error while writing file: ", filename );
            }

            virtual ~VTKOutputImpl() {}

            const Mesh& mesh() const
            {
                return mesh_;
            }

            absl::string_view filename() const
            {
                return filename_;
            }

            void write_attributes( pugi::xml_node& attribute_node,
                const AttributeManager& manager ) const
            {
                absl::FixedArray< index_t > elements( manager.nb_elements() );
                absl::c_iota( elements, 0 );
                write_attributes( attribute_node, manager, elements );
            }

            void write_attributes( pugi::xml_node& attribute_node,
                const AttributeManager& manager,
                absl::Span< const index_t > elements ) const
            {
                for( const auto& name : manager.attribute_names() )
                {
                    const auto attribute =
                        manager.find_generic_attribute( name );
                    if( !attribute || !attribute->is_genericable() )
                    {
                        continue;
                    }
                    auto data_array = write_attribute_header(
                        attribute_node, name, attribute->nb_items() );
                    auto min = std::numeric_limits< float >::max();
                    auto max = std::numeric_limits< float >::lowest();
                    std::string values;
                    for( const auto e : elements )
                    {
                        for( const auto i : LRange{ attribute->nb_items() } )
                        {
                            const auto value =
                                attribute->generic_item_value( e, i );
                            absl::StrAppend( &values, value, " " );
                            min = std::min( min, value );
                            max = std::max( max, value );
                        }
                    }
                    data_array.append_attribute( "RangeMin" ).set_value( min );
                    data_array.append_attribute( "RangeMax" ).set_value( max );
                    data_array.text().set( values.c_str() );
                }
            }

            pugi::xml_node write_attribute_header(
                pugi::xml_node& attribute_node,
                absl::string_view name,
                local_index_t nb_components ) const
            {
                auto data_array = attribute_node.append_child( "DataArray" );
                data_array.append_attribute( "type" ).set_value( "Float64" );
                data_array.append_attribute( "Name" ).set_value(
                    to_string( name ).c_str() );
                data_array.append_attribute( "format" ).set_value( "ascii" );
                data_array.append_attribute( "NumberOfComponents" )
                    .set_value( nb_components );
                return data_array;
            }

        private:
            pugi::xml_node write_root_attributes()
            {
                auto root = document_.append_child( "VTKFile" );
                root.append_attribute( "type" ).set_value( type_ );
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
                write_piece( object );
            }

            virtual void write_piece( pugi::xml_node& object ) = 0;

        private:
            absl::string_view filename_;
            std::ofstream file_;
            const Mesh& mesh_;
            pugi::xml_document document_;
            const char* type_;
        };
    } // namespace detail
} // namespace geode
