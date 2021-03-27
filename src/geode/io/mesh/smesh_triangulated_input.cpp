/*
 * Copyright (c) 2019 - 2021 Geode-solutions
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

#include <geode/io/mesh/private/smesh_triangulated_input.h>

#include <fstream>

#include <absl/strings/str_split.h>

#include <geode/basic/filename.h>

#include <geode/geometry/point.h>

#include <geode/mesh/builder/triangulated_surface_builder.h>
#include <geode/mesh/core/triangulated_surface.h>

namespace
{
    class SMESHTriangulatedInputImpl
    {
    public:
        SMESHTriangulatedInputImpl( absl::string_view filename,
            geode::TriangulatedSurface3D& triangulated_surface )
            : surface_( triangulated_surface ),
              builder_{ geode::TriangulatedSurfaceBuilder3D::create(
                  triangulated_surface ) },
              file_{ filename.data() }
        {
            builder_->set_name( geode::filename_without_extension( filename ) );
        }

        void read_file()
        {
            read_points();
            read_triangles();
        }

    private:
        void read_points()
        {
            const auto header = tokens();
            geode::index_t nb_points;
            auto ok = absl::SimpleAtoi( header.front(), &nb_points );
            OPENGEODE_EXCEPTION( ok, "[SMESHTriangulatedInput::read_points] "
                                     "Cannot read number of points" );
            builder_->create_vertices( nb_points );
            for( const auto p : geode::Range{ nb_points } )
            {
                const auto values = tokens();
                geode::Point3D point;
                for( const auto d : geode::LRange{ 3 } )
                {
                    double coord;
                    ok = absl::SimpleAtod( values[d + 1], &coord );
                    OPENGEODE_EXCEPTION( ok,
                        "[SMESHTriangulatedInput::read_points] "
                        "Cannot read coordinate" );
                    point.set_value( d, coord );
                }
                builder_->set_point( p, point );
            }
        }

        void read_triangles()
        {
            const auto header = tokens();
            geode::index_t nb_triangles;
            auto ok = absl::SimpleAtoi( header.front(), &nb_triangles );
            OPENGEODE_EXCEPTION( ok, "[SMESHTriangulatedInput::read_triangles] "
                                     "Cannot read number of triangles" );
            builder_->reserve_triangles( nb_triangles );
            for( const auto p : geode::Range{ nb_triangles } )
            {
                geode_unused( p );
                const auto values = tokens();
                OPENGEODE_EXCEPTION( values.front() == "3",
                    "[SMESHTriangulatedInput::read_triangles] Only triangles "
                    "can be imported" );
                std::array< geode::index_t, 3 > vertices;
                for( const auto d : geode::LRange{ 3 } )
                {
                    geode::index_t index;
                    ok = absl::SimpleAtoi( values[d + 1], &index );
                    OPENGEODE_EXCEPTION( ok,
                        "[SMESHTriangulatedInput::read_triangles] "
                        "Cannot read triangle vertex index" );
                    vertices[d] = index - 1;
                }
                builder_->create_triangle( vertices );
            }
            builder_->compute_polygon_adjacencies();
        }

        std::vector< absl::string_view > tokens()
        {
            std::getline( file_, line_ );
            return absl::StrSplit( absl::StripAsciiWhitespace( line_ ), " " );
        }

    private:
        geode::TriangulatedSurface3D& surface_;
        std::unique_ptr< geode::TriangulatedSurfaceBuilder3D > builder_;
        std::ifstream file_;
        std::string line_;
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void SMESHTriangulatedInput::do_read()
        {
            SMESHTriangulatedInputImpl impl{ filename(),
                triangulated_surface() };
            impl.read_file();
        }
    } // namespace detail
} // namespace geode
