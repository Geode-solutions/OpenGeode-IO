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

#include <fstream>

#include <absl/strings/str_split.h>

#include <geode/basic/filename.h>

#include <geode/geometry/point.h>

namespace geode
{
    namespace detail
    {
        template < typename Mesh, typename Builder, index_t element >
        class SMESHInputImpl
        {
        public:
            SMESHInputImpl( absl::string_view filename, Mesh& mesh )
                : mesh_( mesh ),
                  builder_{ Builder::create( mesh ) },
                  file_{ to_string( filename ) }
            {
            }

            virtual ~SMESHInputImpl() = default;

            void read_file()
            {
                read_points();
                read_elements();
            }

        protected:
            Builder& builder()
            {
                return *builder_;
            }

        private:
            void read_points()
            {
                const auto header = tokens();
                index_t nb_points;
                auto ok = absl::SimpleAtoi( header.front(), &nb_points );
                OPENGEODE_EXCEPTION( ok, "[SMESHInput::read_points] "
                                         "Cannot read number of points" );
                builder_->create_vertices( nb_points );
                for( const auto p : Range{ nb_points } )
                {
                    const auto values = tokens();
                    index_t index;
                    ok = absl::SimpleAtoi( values[0], &index );
                    OPENGEODE_EXCEPTION( ok, "[SMESHInput::read_points] "
                                             "Cannot read vertex index" );
                    vertices_.emplace( index, p );
                    Point3D point;
                    for( const auto d : LRange{ 3 } )
                    {
                        double coord;
                        ok = absl::SimpleAtod( values[d + 1], &coord );
                        OPENGEODE_EXCEPTION( ok, "[SMESHInput::read_points] "
                                                 "Cannot read coordinate" );
                        point.set_value( d, coord );
                    }
                    builder_->set_point( p, point );
                }
            }

            void read_elements()
            {
                const auto header = tokens();
                index_t nb_edges;
                auto ok = absl::SimpleAtoi( header.front(), &nb_edges );
                OPENGEODE_EXCEPTION( ok, "[SMESHInput::read_elements] "
                                         "Cannot read number of edges" );
                for( const auto e : Range{ nb_edges } )
                {
                    geode_unused( e );
                    const auto values = tokens();
                    std::array< index_t, element > vertices;
                    for( const auto d : LRange{ element } )
                    {
                        index_t index;
                        ok = absl::SimpleAtoi( values[d + 1], &index );
                        OPENGEODE_EXCEPTION( ok,
                            "[SMESHInput::read_elements] "
                            "Cannot read edge vertex index" );
                        vertices[d] = vertices_.at( index );
                    }
                    create_element( vertices );
                }
            }

            virtual void create_element(
                const std::array< index_t, element >& vertices ) = 0;

            std::vector< absl::string_view > tokens()
            {
                std::getline( file_, line_ );
                return absl::StrSplit(
                    absl::StripAsciiWhitespace( line_ ), " " );
            }

        private:
            Mesh& mesh_;
            std::unique_ptr< Builder > builder_;
            std::ifstream file_;
            std::string line_;
            absl::flat_hash_map< index_t, index_t > vertices_;
        };
    } // namespace detail
} // namespace geode
