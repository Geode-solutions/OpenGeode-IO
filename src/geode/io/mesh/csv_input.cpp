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
#include <geode/io/mesh/internal/csv_input.hpp>

#include <geode/mesh/builder/point_set_builder.hpp>
#include <geode/mesh/core/point_set.hpp>

namespace
{
    class CSVInputImpl
    {
    public:
        CSVInputImpl( std::string_view filename )
            : filename_{ filename },
              json_filename_{ geode::to_string( filename.substr(
                                  0, filename.find_last_of( '.' ) ) )
                              + ".json" },
              json_file_{ json_filename_, std::ios::binary }
        {
        }

        std::unique_ptr< geode::PointSet3D > point_set()
        {
            geode::CsvInputHelpers helpers{ filename_ };
            nlohmann::json json;
            json_file_ >> json;
            helpers.set_first_row( json["FirstRow"] );
            helpers.set_header_row( json["HeaderRow"] );
            helpers.set_separator( json["Separator"].get< std::string >()[0] );
            helpers.set_x_column( json["XColumn"] );
            helpers.set_y_column( json["YColumn"] );
            helpers.set_z_column( json["ZColumn"] );
            return helpers.create_point_set();
        }

        geode::AdditionalFiles additional_files()
        {
            DEBUG( "additional files" );
            geode::AdditionalFiles missing;
            if( !geode::file_exists( json_filename_ ) )
            {
                missing.optional_files.emplace_back( json_filename_, false );
                return missing;
            }
            bool contains_all_info{ true };
            nlohmann::json json;
            json_file_ >> json;
            if( !json.contains( "FirstRow" ) || !json.contains( "HeaderRow" )
                || !json.contains( "Separator" ) || !json.contains( "XColumn" )
                || !json.contains( "YColumn" ) || !json.contains( "ZColumn" ) )
            {
                contains_all_info = false;
            }
            missing.optional_files.emplace_back(
                json_filename_, contains_all_info );
            return missing;
        }

    private:
        std::string_view filename_;
        std::string json_filename_;
        std::ifstream json_file_;
    };
} // namespace

namespace geode
{
    namespace internal
    {
        std::unique_ptr< PointSet3D > CSVInput::read( const MeshImpl& impl )
        {
            geode_unused( impl );
            CSVInputImpl reader{ this->filename() };
            return reader.point_set();
        }

        AdditionalFiles CSVInput::additional_files() const
        {
            CSVInputImpl reader{ this->filename() };
            return reader.additional_files();
        }

    } // namespace internal
} // namespace geode