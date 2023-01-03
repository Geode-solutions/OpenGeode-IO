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

#include <geode/io/mesh/private/triangle_output.h>

#include <fstream>

#include <geode/geometry/point.h>

#include <geode/mesh/core/triangulated_surface.h>

namespace
{
    static constexpr char EOL{ '\n' };
    static constexpr char SPACE{ ' ' };

    void write_node(
        const std::string& filename, const geode::TriangulatedSurface2D& mesh )
    {
        std::ofstream node{ filename };
        node << mesh.nb_vertices() << " 2 0 0" << EOL;
        for( const auto v : geode::Range{ mesh.nb_vertices() } )
        {
            node << v << SPACE << mesh.point( v ).string() << EOL;
        }
        node << std::flush;
    }

    void write_ele(
        const std::string& filename, const geode::TriangulatedSurface2D& mesh )
    {
        std::ofstream ele{ filename };
        ele << mesh.nb_polygons() << " 3 0" << EOL;
        for( const auto p : geode::Range{ mesh.nb_polygons() } )
        {
            ele << p;
            for( const auto v : geode::LRange{ 3 } )
            {
                ele << SPACE << mesh.polygon_vertex( { p, v } );
            }
            ele << EOL;
        }
        ele << std::flush;
    }

    void write_neigh(
        const std::string& filename, const geode::TriangulatedSurface2D& mesh )
    {
        std::ofstream neigh{ filename };
        neigh << mesh.nb_polygons() << " 3" << EOL;
        for( const auto p : geode::Range{ mesh.nb_polygons() } )
        {
            neigh << p;
            for( const auto e : geode::LRange{ 3 } )
            {
                neigh << SPACE;
                if( const auto adj = mesh.polygon_adjacent( { p, e } ) )
                {
                    neigh << adj.value();
                }
                else
                {
                    neigh << -1;
                }
            }
            neigh << EOL;
        }
        neigh << std::flush;
    }
} // namespace

namespace geode
{
    namespace detail
    {
        void TriangleOutput::write( const TriangulatedSurface2D& surface ) const
        {
            auto file = filename();
            file.remove_suffix( TriangleOutput::extension().size() + 1 );
            write_node( absl::StrCat( file, ".node" ), surface );
            write_ele( absl::StrCat( file, ".ele" ), surface );
            write_neigh( absl::StrCat( file, ".neigh" ), surface );
        }
    } // namespace detail
} // namespace geode
