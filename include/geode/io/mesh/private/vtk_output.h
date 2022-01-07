/*
 * Copyright (c) 2019 - 2022 Geode-solutions
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

#include <geode/io/mesh/detail/common.h>

#include <fstream>

#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <pugixml.hpp>
#include <zlib-ng.h>

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
                    "[VTKOutput] Error while opening file: ", filename );
            }

            virtual ~VTKOutputImpl()
            {
                const auto ok =
                    document_.save_file( to_string( filename_ ).c_str() );
                OPENGEODE_EXCEPTION(
                    ok, "[VTKOutput] Error while writing file: ", filename_ );
            }

            const Mesh& mesh() const
            {
                return mesh_;
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
                auto piece = object.append_child( "Piece" );
                write_piece( piece );
            }

            virtual void write_piece( pugi::xml_node& piece ) = 0;

        private:
            absl::string_view filename_;
            std::ofstream file_;
            const Mesh& mesh_;
            pugi::xml_document document_;
            const char* type_;
        };
    } // namespace detail
} // namespace geode
