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

#include <cmath>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/strings/match.h>
#include <absl/strings/str_join.h>
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
            : filename_{ filename },
              file_{ geode::to_string( filename ), std::ios::binary }
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
            auto& attribute_manager = point_set->vertex_attribute_manager();
            const auto max_coordinate_column =
                std::max( { x_column_, y_column_, z_column_ } );
            static constexpr index_t MAX_REPORTED_ROWS{ 10 };
            std::vector< index_t > short_rows;
            index_t nb_short_rows{ 0 };
            std::vector< index_t > invalid_rows;
            index_t nb_invalid_rows{ 0 };
            while( std::getline( file_, line ) )
            {
                if( current_row == 0
                    && absl::StartsWith( line, UTF8_BYTE_ORDER_MARK ) )
                {
                    line.erase( 0, UTF8_BYTE_ORDER_MARK.size() );
                }
                if( current_row == header_row_ )
                {
                    headers_ = split_line( line );
                    current_row++;
                    continue;
                }
                if( current_row < first_row_ )
                {
                    current_row++;
                    continue;
                }
                if( absl::StripAsciiWhitespace( line ).empty() )
                {
                    current_row++;
                    continue;
                }
                const auto line_values = split_line( line );
                if( line_values.size() <= max_coordinate_column )
                {
                    if( short_rows.size() < MAX_REPORTED_ROWS )
                    {
                        short_rows.push_back( current_row + 1 );
                    }
                    nb_short_rows++;
                    current_row++;
                    continue;
                }
                const auto point = read_point( line_values );
                if( !point )
                {
                    if( invalid_rows.size() < MAX_REPORTED_ROWS )
                    {
                        invalid_rows.push_back( current_row + 1 );
                    }
                    nb_invalid_rows++;
                    current_row++;
                    continue;
                }
                const auto vertex_id = builder->create_point( point.value() );
                set_attribute_on_vertex(
                    vertex_id, line_values, attribute_manager );
                current_row++;
            }
            if( nb_short_rows != 0 )
            {
                Logger::warning( "[CSVInput] Skipped ", nb_short_rows,
                    " rows without the coordinate column ",
                    max_coordinate_column, " in file ", filename_,
                    " (rows: ", absl::StrJoin( short_rows, ", " ),
                    nb_short_rows > short_rows.size() ? ", ..." : "", ")" );
            }
            if( nb_invalid_rows != 0 )
            {
                Logger::warning( "[CSVInput] Skipped ", nb_invalid_rows,
                    " rows with non numeric or non finite coordinates in "
                    "file ",
                    filename_, " (rows: ", absl::StrJoin( invalid_rows, ", " ),
                    nb_invalid_rows > invalid_rows.size() ? ", ..." : "", ")" );
            }
            for( const auto& [col, nb_invalid] : invalid_cells_ )
            {
                if( !double_attrs_.contains( col ) )
                {
                    Logger::info( "[CSVInput] Column ", col,
                        " has no numeric value and is ignored in file ",
                        filename_ );
                    continue;
                }
                Logger::warning( "[CSVInput] ", nb_invalid,
                    " non numeric values of attribute \"",
                    attribute_names_.at( col ), "\" replaced by NaN in file ",
                    filename_ );
            }
            return point_set;
        }

    private:
        std::optional< Point3D > read_point(
            const std::vector< std::string >& line_values ) const
        {
            Point3D point;
            const std::array< index_t, 3 > columns{ x_column_, y_column_,
                z_column_ };
            for( const auto d : LRange{ 3 } )
            {
                double value;
                if( !absl::SimpleAtod( line_values[columns[d]], &value )
                    || !std::isfinite( value ) )
                {
                    return std::nullopt;
                }
                point.set_value( d, value );
            }
            return point;
        }

        /*!
         * Split a row on the separator, following RFC 4180 quoting: a field
         * may be enclosed in double quotes to hold separators, and a double
         * quote inside a quoted field is escaped by doubling it.
         */
        std::vector< std::string > split_line( std::string_view line ) const
        {
            line = absl::StripTrailingAsciiWhitespace( line );
            if( line.find( '"' ) == std::string_view::npos )
            {
                return absl::StrSplit( line, absl::ByChar( separator_ ) );
            }
            std::vector< std::string > fields;
            std::string field;
            bool in_quotes{ false };
            for( size_t i = 0; i < line.size(); i++ )
            {
                const auto character = line[i];
                if( character == '"' )
                {
                    if( in_quotes && i + 1 < line.size() && line[i + 1] == '"' )
                    {
                        field.push_back( '"' );
                        i++;
                    }
                    else
                    {
                        in_quotes = !in_quotes;
                    }
                }
                else if( character == separator_ && !in_quotes )
                {
                    fields.push_back( std::move( field ) );
                    field.clear();
                }
                else
                {
                    field.push_back( character );
                }
            }
            fields.push_back( std::move( field ) );
            return fields;
        }

        void set_attribute_on_vertex( const geode::index_t vertex_id,
            const std::vector< std::string >& line_values,
            AttributeManager& attribute_manager )
        {
            for( const auto col : geode::Range{ line_values.size() } )
            {
                if( col == x_column_ || col == y_column_ || col == z_column_ )
                {
                    continue;
                }
                double value;
                const auto is_number =
                    absl::SimpleAtod( line_values[col], &value );
                auto attribute_it = double_attrs_.find( col );
                if( attribute_it == double_attrs_.end() )
                {
                    if( !is_number )
                    {
                        invalid_cells_[col]++;
                        continue;
                    }
                    // Attributes are created on their first numeric value:
                    // previous vertices keep the NaN default value
                    attribute_it = double_attrs_
                                       .emplace( col, create_attribute( col,
                                                          attribute_manager ) )
                                       .first;
                }
                if( is_number )
                {
                    attribute_it->second->set_value( vertex_id, value );
                }
                else
                {
                    // The vertex keeps the NaN default value
                    invalid_cells_[col]++;
                }
            }
        }

        std::shared_ptr< VariableAttribute< double > > create_attribute(
            index_t col, AttributeManager& attribute_manager )
        {
            std::string name;
            if( col < headers_.size() )
            {
                name = absl::StripAsciiWhitespace( headers_[col] );
            }
            if( name.empty() )
            {
                name = absl::StrCat( "column_", col );
            }
            if( name_exists( name, attribute_manager ) )
            {
                auto new_name = absl::StrCat( name, "_", col );
                while( name_exists( new_name, attribute_manager ) )
                {
                    absl::StrAppend( &new_name, "_" );
                }
                Logger::warning( "[CSVInput] Attribute name \"", name,
                    "\" of column ", col, " is already used, renamed \"",
                    new_name, "\" in file ", filename_ );
                name = std::move( new_name );
            }
            AttributeValues< double > default_values;
            default_values.default_value =
                std::numeric_limits< double >::quiet_NaN();
            default_values.no_value =
                std::numeric_limits< double >::quiet_NaN();
            AttributeProperties properties;
            properties.assignable = false;
            properties.interpolable = false;
            properties.transferable = true;
            const auto attribute_id =
                attribute_manager.create_attribute< VariableAttribute, double >(
                    name, default_values, properties );
            attribute_names_[col] = name;
            return attribute_manager
                .find_attribute< VariableAttribute, double >( attribute_id );
        }

        static bool name_exists(
            std::string_view name, const AttributeManager& attribute_manager )
        {
            const auto ids =
                attribute_manager.attribute_ids_matching_name( name );
            return ids && !ids->empty();
        }

    private:
        static constexpr std::string_view UTF8_BYTE_ORDER_MARK{
            "\xEF\xBB\xBF"
        };
        std::vector< std::string > headers_;
        absl::flat_hash_map< index_t,
            std::shared_ptr< VariableAttribute< double > > >
            double_attrs_;
        absl::flat_hash_map< index_t, index_t > invalid_cells_;
        absl::flat_hash_map< index_t, std::string > attribute_names_;
        std::string filename_;
        std::ifstream file_;
        char separator_{ ',' };
        index_t x_column_{ 0 };
        index_t y_column_{ 1 };
        index_t z_column_{ 2 };
        index_t header_row_{ 0 };
        index_t first_row_{ 1 };
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