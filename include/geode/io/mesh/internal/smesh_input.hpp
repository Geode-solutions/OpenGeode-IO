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

#include <absl/strings/str_split.h>

#include <geode/basic/filename.hpp>
#include <geode/basic/string.hpp>

#include <geode/geometry/point.hpp>

namespace geode
{
    namespace internal
    {
        template < typename Mesh, index_t element >
        class SMESHInputImpl
        {
        public:
            using Builder = typename Mesh::Builder;

            SMESHInputImpl( std::string_view filename, Mesh& mesh )
                : mesh_( mesh ),
                  builder_{ Builder::create( mesh ) },
                  file_{ to_string( filename ) }
            {
            }

            virtual ~SMESHInputImpl() = default;

            Percentage is_loadable()
            {
                const auto nb_points = string_to_index( tokens().front() );
                for( const auto p : Range{ nb_points } )
                {
                    geode_unused( p );
                    std::getline( file_, line_ );
                }
                absl::flat_hash_map< index_t, index_t > elements;
                const auto nb_elements = string_to_index( tokens().front() );
                for( const auto e : Range{ nb_elements } )
                {
                    geode_unused( e );
                    const auto values = tokens();
                    elements[string_to_index( values[0] )]++;
                }
                const auto element_it = elements.find( element );
                if( element_it == elements.end() )
                {
                    return geode::Percentage{ 0 };
                }
                return geode::Percentage{
                    static_cast< double >( element_it->second ) / nb_elements
                };
            }

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
                const auto nb_points = string_to_index( header.front() );
                builder_->create_vertices( nb_points );
                for( const auto p : Range{ nb_points } )
                {
                    const auto values = tokens();
                    vertices_.emplace( string_to_index( values[0] ), p );
                    Point3D point;
                    for( const auto d : LRange{ 3 } )
                    {
                        point.set_value( d, string_to_double( values[d + 1] ) );
                    }
                    builder_->set_point( p, point );
                }
            }

            void read_elements()
            {
                const auto header = tokens();
                const auto nb_elements = string_to_index( header.front() );
                for( const auto e : Range{ nb_elements } )
                {
                    geode_unused( e );
                    const auto values = tokens();
                    std::array< index_t, element > vertices;
                    for( const auto d : LRange{ element } )
                    {
                        vertices[d] =
                            vertices_.at( string_to_index( values[d + 1] ) );
                    }
                    create_element( vertices );
                }
            }

            virtual void create_element(
                const std::array< index_t, element >& vertices ) = 0;

            std::vector< std::string_view > tokens()
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
    } // namespace internal
} // namespace geode
