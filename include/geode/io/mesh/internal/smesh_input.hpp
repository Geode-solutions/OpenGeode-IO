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

#include <absl/strings/str_split.h>

#include <geode/basic/filename.hpp>
#include <geode/basic/string.hpp>

#include <geode/geometry/point.hpp>

#include <geode/io/mesh/common.hpp>

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
                : filename_{ filename },
                  mesh_( mesh ),
                  builder_{ Builder::create( mesh ) },
                  file_{ to_string( filename ) }
            {
            }

            virtual ~SMESHInputImpl() = default;

            Percentage is_loadable()
            {
                const auto nb_points = read_count( "points" );
                for( const auto p : Range{ nb_points } )
                {
                    geode_unused( p );
                    tokens();
                }
                absl::flat_hash_map< index_t, index_t > elements;
                const auto nb_elements = read_count( "elements" );
                for( const auto e : Range{ nb_elements } )
                {
                    geode_unused( e );
                    const auto values = tokens();
                    check_nb_tokens( values, 1, "an element" );
                    elements[parse_index( values[0] )]++;
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
                const auto nb_points = read_count( "points" );
                builder_->create_vertices( nb_points );
                for( const auto p : Range{ nb_points } )
                {
                    const auto values = tokens();
                    check_nb_tokens( values, 4, "a point" );
                    vertices_.emplace( parse_index( values[0] ), p );
                    Point3D point;
                    for( const auto d : LRange{ 3 } )
                    {
                        point.set_value( d, parse_double( values[d + 1] ) );
                    }
                    builder_->set_point( p, point );
                }
            }

            void read_elements()
            {
                const auto nb_elements = read_count( "elements" );
                for( const auto e : Range{ nb_elements } )
                {
                    geode_unused( e );
                    const auto values = tokens();
                    check_nb_tokens( values, element + 1, "an element" );
                    std::array< index_t, element > vertices;
                    for( const auto d : LRange{ element } )
                    {
                        vertices[d] =
                            vertices_.at( parse_index( values[d + 1] ) );
                    }
                    create_element( vertices );
                }
            }

            virtual void create_element(
                const std::array< index_t, element >& vertices ) = 0;

            index_t read_count( std::string_view what )
            {
                const auto header = tokens();
                check_nb_tokens(
                    header, 1, absl::StrCat( "the number of ", what ) );
                return parse_index( header.front() );
            }

            void check_nb_tokens( const std::vector< std::string_view >& values,
                size_t nb_expected,
                std::string_view what ) const
            {
                OpenGeodeIOMeshException::check_exception(
                    values.size() >= nb_expected, nullptr,
                    OpenGeodeException::TYPE::data, "[SMESHInput] ",
                    values.empty() ? "Unexpected end of file "
                                   : "Not enough values ",
                    "while reading ", what, " (expected at least ", nb_expected,
                    ", found ", values.size(), ") in file ", filename_,
                    " at line ", line_number_ );
            }

            index_t parse_index( std::string_view token ) const
            {
                index_t value{ 0 };
                OpenGeodeIOMeshException::check_exception(
                    absl::SimpleAtoi( token, &value ), nullptr,
                    OpenGeodeException::TYPE::data,
                    "[SMESHInput] Cannot convert \"", token,
                    "\" to index in file ", filename_, " at line ",
                    line_number_ );
                return value;
            }

            double parse_double( std::string_view token ) const
            {
                double value{ 0 };
                OpenGeodeIOMeshException::check_exception(
                    absl::SimpleAtod( token, &value ), nullptr,
                    OpenGeodeException::TYPE::data,
                    "[SMESHInput] Cannot convert \"", token,
                    "\" to number in file ", filename_, " at line ",
                    line_number_ );
                return value;
            }

            /*!
             * Read the next line holding data, skipping blank lines and
             * comments (everything after a '#'), and split it into tokens.
             * Return an empty vector at the end of the file.
             */
            std::vector< std::string_view > tokens()
            {
                while( std::getline( file_, line_ ) )
                {
                    line_number_++;
                    std::string_view content{ line_ };
                    content = content.substr( 0, content.find( '#' ) );
                    std::vector< std::string_view > result =
                        absl::StrSplit( content, absl::ByAnyChar( " \t\r" ),
                            absl::SkipEmpty() );
                    if( !result.empty() )
                    {
                        return result;
                    }
                }
                return {};
            }

        private:
            std::string filename_;
            Mesh& mesh_;
            std::unique_ptr< Builder > builder_;
            std::ifstream file_;
            std::string line_;
            index_t line_number_{ 0 };
            absl::flat_hash_map< index_t, index_t > vertices_;
        };
    } // namespace internal
} // namespace geode
