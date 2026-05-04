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

#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/match.h>
#include <absl/strings/str_split.h>

#include <geode/geometry/point.hpp>

#include <geode/basic/attribute.hpp>
#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/file.hpp>
#include <geode/basic/filename.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/pimpl_impl.hpp>
#include <geode/basic/string.hpp>
#include <geode/basic/variable_attribute.hpp>

#include <geode/io/mesh/csv_input_helpers.hpp>
#include <geode/mesh/builder/point_set_builder.hpp>
#include <geode/mesh/core/point_set.hpp>

namespace geode
{

    class CsvInputHelpers::Impl
    {
    public:
        Impl( std::string_view filename )
            : file_{ geode::to_string( filename ), std::ios::binary },
              separator_( ',' ),
              x_column_( 0 ),
              y_column_( 1 ),
              z_column_( 2 ),
              header_row_( 0 ),
              first_row_( 1 )
        {
        }

        void set_separator( char separator )
        {
            separator_ = separator;
        }
        void set_x_column( index_t x_column )
        {
            x_column_ = x_column;
        }
        void set_y_column( index_t y_column )
        {
            y_column_ = y_column;
        }
        void set_z_column( index_t z_column )
        {
            z_column_ = z_column;
        }
        void set_header_row( index_t header_row )
        {
            header_row_ = header_row;
        }
        void set_first_row( index_t first_row )
        {
            first_row_ = first_row;
        }

        std::unique_ptr< PointSet3D > create_point_set()
        {
            auto point_set = PointSet3D::create();
            auto builder = PointSetBuilder3D::create( *point_set );
            std::string line;
            index_t current_row{ 0 };
            std::vector< std::string > headers;
            absl::flat_hash_map< index_t,
                std::shared_ptr< VariableAttribute< double > > >
                double_attrs;
            auto& attribute_manager = point_set->vertex_attribute_manager();
            while( std::getline( file_, line ) )
            {
                if( current_row == header_row_ )
                {
                    headers = split_line( line );
                    current_row++;
                    continue;
                }
                if( current_row < first_row_ )
                {
                    current_row++;
                    continue;
                }
                const auto line_values = split_line( line );
                if( line_values.empty() )
                {
                    current_row++;
                    continue;
                }
                const auto x = string_to_double( line_values[x_column_] );
                const auto y = string_to_double( line_values[y_column_] );
                const auto z = string_to_double( line_values[z_column_] );
                const auto vertex_id =
                    builder->create_point( geode::Point3D{ { x, y, z } } );
                set_attribute_on_vertex( vertex_id, line_values, headers,
                    double_attrs, attribute_manager );
                current_row++;
            }

            return point_set;
        }

    private:
        std::vector< std::string > split_line( const std::string& line )
        {
            return absl::StrSplit( absl::StripAsciiWhitespace( line ),
                absl::ByChar( separator_ ) );
        }

        void set_attribute_on_vertex( const geode::index_t vertex_id,
            const std::vector< std::string >& line_values,
            const std::vector< std::string >& headers,
            absl::flat_hash_map< index_t,
                std::shared_ptr< VariableAttribute< double > > >& double_attrs,
            AttributeManager& attribute_manager )
        {
            if( vertex_id == 0 )
            {
                for( const auto col : geode::Range{ line_values.size() } )
                {
                    if( col == x_column_ || col == y_column_
                        || col == z_column_ )
                    {
                        continue;
                    }
                    const auto attribute_name = headers.at( col );
                    double value{ 0 };
                    if( !absl::SimpleAtod( line_values[col], &value ) )
                    {
                        continue;
                    }
                    double_attrs[col] =
                        attribute_manager.find_or_create_attribute<
                            VariableAttribute, double >( attribute_name, 0.0 );
                }
            }
            for( const auto col : geode::Range{ line_values.size() } )
            {
                if( col == x_column_ || col == y_column_ || col == z_column_ )
                {
                    continue;
                }
                if( double_attrs.contains( col ) )
                {
                    double_attrs[col]->set_value(
                        vertex_id, string_to_double( line_values[col] ) );
                }
            }
        }

    private:
        std::ifstream file_;
        char separator_;
        index_t x_column_;
        index_t y_column_;
        index_t z_column_;
        index_t header_row_;
        index_t first_row_;
    };

    CsvInputHelpers::CsvInputHelpers( std::string_view filename )
        : impl_( filename )
    {
    }

    CsvInputHelpers::CsvInputHelpers( CsvInputHelpers&& other ) = default;

    CsvInputHelpers::~CsvInputHelpers() = default;

    void CsvInputHelpers::set_separator( char separator )
    {
        impl_->set_separator( separator );
    }

    void CsvInputHelpers::set_x_column( index_t x_column )
    {
        impl_->set_x_column( x_column );
    }

    void CsvInputHelpers::set_y_column( index_t y_column )
    {
        impl_->set_y_column( y_column );
    }

    void CsvInputHelpers::set_z_column( index_t z_column )
    {
        impl_->set_z_column( z_column );
    }

    void CsvInputHelpers::set_header_row( index_t header_row )
    {
        impl_->set_header_row( header_row );
    }

    void CsvInputHelpers::set_first_row( index_t first_row )
    {
        impl_->set_first_row( first_row );
    }

    std::unique_ptr< PointSet3D > CsvInputHelpers::create_point_set()
    {
        return impl_->create_point_set();
    }

} // namespace geode