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

#include <geode/io/mesh/internal/csv_input.hpp>

#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

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

namespace
{
    struct CSVSettings
    {
        geode::index_t first_row{ 1 };
        geode::index_t header_row{ 0 };
        char separator{ ',' };
        geode::index_t x_column{ 0 };
        geode::index_t y_column{ 1 };
        geode::index_t z_column{ 2 };
    };

    class CSVInputImpl
    {
    public:
        explicit CSVInputImpl( std::string_view filename )
            : filename_{ filename },
              json_filename_{ geode::to_string( filename.substr(
                                  0, filename.find_last_of( '.' ) ) )
                              + ".json" }
        {
        }

        std::unique_ptr< geode::PointSet3D > point_set()
        {
            const auto settings = read_settings();
            geode::CsvInputHelpers helpers{ filename_ };
            helpers.set_first_row( settings.first_row );
            helpers.set_header_row( settings.header_row );
            helpers.set_separator( settings.separator );
            helpers.set_x_column( settings.x_column );
            helpers.set_y_column( settings.y_column );
            helpers.set_z_column( settings.z_column );
            return helpers.create_point_set();
        }

        geode::AdditionalFiles additional_files()
        {
            geode::AdditionalFiles files;
            files.mandatory_files.emplace_back(
                json_filename_, !geode::file_exists( json_filename_ ) );
            return files;
        }

        geode::Percentage is_loadable()
        {
            if( !geode::file_exists( json_filename_ ) )
            {
                return geode::Percentage{ 0 };
            }
            try
            {
                read_settings();
            }
            catch( const geode::OpenGeodeException& )
            {
                return geode::Percentage{ 0 };
            }
            return geode::Percentage{ 1 };
        }

    private:
        CSVSettings read_settings() const
        {
            std::ifstream json_file{ json_filename_, std::ios::binary };
            geode::OpenGeodeIOMeshException::check_exception( json_file.good(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[CSVInput] Cannot open settings file ", json_filename_ );
            const auto json = nlohmann::json::parse( json_file, nullptr,
                /* allow_exceptions */ false );
            geode::OpenGeodeIOMeshException::check_exception(
                !json.is_discarded() && json.is_object(), nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[CSVInput] Settings file ", json_filename_,
                " is not a valid JSON object" );
            CSVSettings settings;
            settings.first_row = read_index( json, "firstRow" );
            settings.header_row = read_index( json, "headerRow" );
            settings.x_column = read_index( json, "xColumn" );
            settings.y_column = read_index( json, "yColumn" );
            settings.z_column = read_index( json, "zColumn" );
            check_key( json, "separator" );
            const auto& separator = json["separator"];
            geode::OpenGeodeIOMeshException::check_exception(
                separator.is_string()
                    && separator.get_ref< const std::string& >().size() == 1,
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[CSVInput] Key \"separator\" of settings file ",
                json_filename_, " should be a single character" );
            settings.separator =
                separator.get_ref< const std::string& >().front();
            return settings;
        }

        void check_key( const nlohmann::json& json, std::string_view key ) const
        {
            geode::OpenGeodeIOMeshException::check_exception(
                json.contains( key ), nullptr,
                geode::OpenGeodeException::TYPE::data, "[CSVInput] Key \"", key,
                "\" is missing in settings file ", json_filename_ );
        }

        geode::index_t read_index(
            const nlohmann::json& json, std::string_view key ) const
        {
            check_key( json, key );
            const auto& value = json[std::string{ key }];
            geode::OpenGeodeIOMeshException::check_exception(
                value.is_number_unsigned()
                    && value.get< std::uint64_t >()
                           <= std::numeric_limits< geode::index_t >::max(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[CSVInput] Key \"", key, "\" of settings file ",
                json_filename_, " should be a non-negative integer" );
            return value.get< geode::index_t >();
        }

    private:
        std::string_view filename_;
        std::string json_filename_;
    };
} // namespace

namespace geode::internal
{
    std::unique_ptr< PointSet3D > CSVInput::read( const MeshImpl& impl )
    {
        geode_unused( impl );
        CSVInputImpl reader{ filename() };
        return reader.point_set();
    }

    AdditionalFiles CSVInput::additional_files() const
    {
        CSVInputImpl reader{ filename() };
        return reader.additional_files();
    }

    Percentage CSVInput::is_loadable() const
    {
        CSVInputImpl reader{ filename() };
        return reader.is_loadable();
    }
} // namespace geode::internal