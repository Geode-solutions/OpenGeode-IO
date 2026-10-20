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

#include <filesystem>
#include <fstream>

#include <absl/strings/str_split.h>

#include <geode/basic/filename.hpp>
#include <geode/basic/logger.hpp>
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
                    if( tokens().empty() )
                    {
                        return Percentage{ 0 };
                    }
                }
                absl::flat_hash_map< index_t, index_t > elements;
                // Only sample the first elements to keep this check cheap
                static constexpr index_t NB_SAMPLED_ELEMENTS{ 100 };
                const auto nb_elements = read_count( "elements" );
                index_t nb_read_elements{ 0 };
                for( const auto e :
                    Range{ std::min( nb_elements, NB_SAMPLED_ELEMENTS ) } )
                {
                    geode_unused( e );
                    const auto& values = tokens();
                    if( values.empty() )
                    {
                        break;
                    }
                    elements[parse_index( values[0] )]++;
                    nb_read_elements++;
                }
                const auto element_it = elements.find( element );
                if( element_it == elements.end() )
                {
                    return Percentage{ 0 };
                }
                return Percentage{ static_cast< double >( element_it->second )
                                   / nb_read_elements };
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
                OpenGeodeIOMeshException::check_exception( nb_points != 0,
                    nullptr, OpenGeodeException::TYPE::data,
                    "[SMESHInput] No point in file ", filename_,
                    ": points stored in a separate .node file are not "
                    "supported" );
                // The header count may be corrupted: never trust it more than
                // the number of lines the file can hold
                std::vector< Point3D > points;
                index_t nb_duplicated{ 0 };
                points.reserve( std::min< std::uintmax_t >(
                    nb_points, max_nb_data_lines() ) );
                vertices_.reserve( points.capacity() );
                for( const auto p : Range{ nb_points } )
                {
                    geode_unused( p );
                    const auto& values = tokens();
                    if( is_truncated( values, 4 ) )
                    {
                        Logger::warning( "[SMESHInput] File ", filename_,
                            " is truncated: read ", points.size(), " of ",
                            nb_points, " points" );
                        break;
                    }
                    check_nb_tokens( values, 4, "a point" );
                    if( !vertices_
                            .emplace( parse_index( values[0] ),
                                static_cast< index_t >( points.size() ) )
                            .second )
                    {
                        nb_duplicated++;
                        continue;
                    }
                    Point3D point;
                    for( const auto d : LRange{ 3 } )
                    {
                        point.set_value( d, parse_double( values[d + 1] ) );
                    }
                    points.push_back( point );
                }
                if( nb_duplicated != 0 )
                {
                    Logger::warning( "[SMESHInput] Skipped ", nb_duplicated,
                        " points with an already used id in file ", filename_ );
                }
                builder_->create_vertices(
                    static_cast< index_t >( points.size() ) );
                for( const auto p : Indices{ points } )
                {
                    builder_->set_point( p, points[p] );
                }
            }

            void read_elements()
            {
                const auto& header = tokens();
                if( is_truncated( header, 1 ) )
                {
                    Logger::warning( "[SMESHInput] File ", filename_,
                        " is truncated: no element read" );
                    return;
                }
                check_nb_tokens( header, 1, "the number of elements" );
                const auto nb_elements = parse_index( header.front() );
                index_t nb_read_elements{ 0 };
                index_t nb_wrong_size{ 0 };
                index_t nb_unknown_vertex{ 0 };
                index_t nb_degenerate{ 0 };
                for( const auto e : Range{ nb_elements } )
                {
                    geode_unused( e );
                    const auto& values = tokens();
                    if( values.empty()
                        || ( file_.eof()
                             && ( values.size() < 2
                                  || values.size()
                                         < parse_index( values[0] ) + 1 ) ) )
                    {
                        Logger::warning( "[SMESHInput] File ", filename_,
                            " is truncated: read ", nb_read_elements, " of ",
                            nb_elements, " elements" );
                        break;
                    }
                    nb_read_elements++;
                    check_nb_tokens( values, 1, "an element" );
                    const auto nb_element_vertices = parse_index( values[0] );
                    check_nb_tokens(
                        values, nb_element_vertices + 1, "an element" );
                    if( nb_element_vertices != element )
                    {
                        nb_wrong_size++;
                        continue;
                    }
                    std::array< index_t, element > vertices;
                    bool known_vertices{ true };
                    for( const auto d : LRange{ element } )
                    {
                        const auto vertex_it =
                            vertices_.find( parse_index( values[d + 1] ) );
                        if( vertex_it == vertices_.end() )
                        {
                            known_vertices = false;
                            break;
                        }
                        vertices[d] = vertex_it->second;
                    }
                    if( !known_vertices )
                    {
                        nb_unknown_vertex++;
                        continue;
                    }
                    if( is_degenerate( vertices ) )
                    {
                        nb_degenerate++;
                        continue;
                    }
                    create_element( vertices );
                }
                if( nb_unknown_vertex != 0 )
                {
                    Logger::warning( "[SMESHInput] Skipped ", nb_unknown_vertex,
                        " elements referencing unknown point ids in file ",
                        filename_ );
                }
                if( nb_degenerate != 0 )
                {
                    Logger::warning( "[SMESHInput] Skipped ", nb_degenerate,
                        " elements with repeated points in file ", filename_ );
                }
                if( nb_wrong_size == 0 )
                {
                    return;
                }
                OpenGeodeIOMeshException::check_exception(
                    nb_wrong_size != nb_read_elements, nullptr,
                    OpenGeodeException::TYPE::data, "[SMESHInput] None of the ",
                    nb_read_elements, " elements of file ", filename_, " has ",
                    element, " vertices" );
                Logger::warning( "[SMESHInput] Skipped ", nb_wrong_size, " of ",
                    nb_read_elements, " elements not having ", element,
                    " vertices in file ", filename_ );
            }

            virtual void create_element(
                const std::array< index_t, element >& vertices ) = 0;

            static bool is_degenerate(
                const std::array< index_t, element >& vertices )
            {
                for( const auto v0 : LRange{ element } )
                {
                    for( const auto v1 : LRange{ v0 + 1, element } )
                    {
                        if( vertices[v0] == vertices[v1] )
                        {
                            return true;
                        }
                    }
                }
                return false;
            }

            /*!
             * A missing or incomplete last line means that the file was cut
             */
            bool is_truncated( const std::vector< std::string_view >& values,
                size_t nb_expected ) const
            {
                return values.size() < nb_expected && file_.eof();
            }

            std::uintmax_t max_nb_data_lines() const
            {
                std::error_code error;
                const auto size =
                    std::filesystem::file_size( filename_, error );
                if( error )
                {
                    return 0;
                }
                // Shortest point line: "0 0 0 0\n"
                static constexpr std::uintmax_t MIN_LINE_SIZE{ 8 };
                return size / MIN_LINE_SIZE;
            }

            index_t read_count( std::string_view what )
            {
                const auto& header = tokens();
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
            const std::vector< std::string_view >& tokens()
            {
                tokens_.clear();
                while( std::getline( file_, line_ ) )
                {
                    line_number_++;
                    std::string_view content{ line_ };
                    content = content.substr( 0, content.find( '#' ) );
                    for( const auto token : absl::StrSplit( content,
                             absl::ByAnyChar( " \t\r" ), absl::SkipEmpty() ) )
                    {
                        tokens_.push_back( token );
                    }
                    if( !tokens_.empty() )
                    {
                        break;
                    }
                }
                return tokens_;
            }

        private:
            std::string filename_;
            Mesh& mesh_;
            std::unique_ptr< Builder > builder_;
            std::ifstream file_;
            std::string line_;
            std::vector< std::string_view > tokens_;
            index_t line_number_{ 0 };
            absl::flat_hash_map< index_t, index_t > vertices_;
        };
    } // namespace internal
} // namespace geode
