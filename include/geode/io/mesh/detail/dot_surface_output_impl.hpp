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

#pragma once

#include <fstream>

#include <absl/strings/str_replace.h>

#include <geode/basic/filename.hpp>
#include <geode/basic/uuid.hpp>

#include <geode/mesh/core/surface_edges.hpp>

#include <geode/io/mesh/common.hpp>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class DotSurfaceOutputImpl
        {
        public:
            DotSurfaceOutputImpl(
                std::string_view filename, const Mesh& surface )
                : filename_( filename ),
                  file_( to_string( filename ) ),
                  surface_( surface ),
                  edges_were_enabled_( surface.are_edges_enabled() )
            {
                OpenGeodeIOMeshException::check_exception( file_.good(),
                    nullptr, OpenGeodeException::TYPE::data,
                    "[DotOutput] Error while opening file: ", filename_ );
                surface_.enable_edges();
            }

            ~DotSurfaceOutputImpl()
            {
                // Edges are only enabled for the export, restore the
                // caller's mesh state
                if( !edges_were_enabled_ )
                {
                    surface_.disable_edges();
                }
            }

            DotSurfaceOutputImpl( const DotSurfaceOutputImpl& ) = delete;
            DotSurfaceOutputImpl& operator=(
                const DotSurfaceOutputImpl& ) = delete;

            void write_file()
            {
                // Quoted identifier: names may contain any character
                const auto name =
                    surface_.name().value_or( surface_.id().string() );
                file_ << "graph \""
                      << absl::StrReplaceAll(
                             name, { { "\\", "\\\\" }, { "\"", "\\\"" } } )
                      << "\" {\n";
                const auto& edges = surface_.edges();
                for( const auto edge : Range{ edges.nb_edges() } )
                {
                    const auto& vertices = edges.edge_vertices( edge );
                    file_ << "    " << vertices[0] << " -- " << vertices[1]
                          << ";\n";
                }
                file_ << "}\n";
                file_.close();
                OpenGeodeIOMeshException::check_exception( !file_.fail(),
                    nullptr, OpenGeodeException::TYPE::data,
                    "[DotOutput] Error while writing file: ", filename_ );
            }

        private:
            std::string_view filename_;
            std::ofstream file_;
            const Mesh& surface_;
            bool edges_were_enabled_;
        };
    } // namespace detail
} // namespace geode
