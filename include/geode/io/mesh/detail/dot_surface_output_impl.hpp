/*
 * Copyright (c) 2019 - 2025 Geode-solutions
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

#include <geode/basic/filename.hpp>

#include <geode/mesh/core/surface_edges.hpp>

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
                  surface_( surface )
            {
                surface_.enable_edges();
            }

            void write_file()
            {
                file_ << "graph " << surface_.name() << " {\n";
                const auto& edges = surface_.edges();
                for( const auto edge : Range{ edges.nb_edges() } )
                {
                    const auto& vertices = edges.edge_vertices( edge );
                    file_ << "    " << vertices[0] << " -- " << vertices[1]
                          << ";\n";
                }
                file_ << "}\n";
                file_.close();
            }

        private:
            std::string_view filename_;
            std::ofstream file_;
            const Mesh& surface_;
        };
    } // namespace detail
} // namespace geode
