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
    class CSVInputImpl
    {
    public:
        explicit CSVInputImpl( std::string_view filename )
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
            helpers.set_first_row( json["firstRow"] );
            helpers.set_header_row( json["headerRow"] );
            helpers.set_separator( json["separator"].get< std::string >()[0] );
            helpers.set_x_column( json["xColumn"] );
            helpers.set_y_column( json["yColumn"] );
            helpers.set_z_column( json["zColumn"] );
            return helpers.create_point_set();
        }

        geode::AdditionalFiles additional_files()
        {
            geode::AdditionalFiles missing;
            if( is_loadable().value() == 0 )
            {
                missing.mandatory_files.emplace_back( json_filename_, false );
                return missing;
            }
            missing.mandatory_files.emplace_back( json_filename_, true );
            return missing;
        }

        geode::Percentage is_loadable()
        {
            if( !geode::file_exists( json_filename_ ) )
            {
                return geode::Percentage{ 0 };
            }
            nlohmann::json json;
            json_file_ >> json;
            if( !json.contains( "firstRow" ) || !json.contains( "headerRow" )
                || !json.contains( "separator" ) || !json.contains( "xColumn" )
                || !json.contains( "yColumn" ) || !json.contains( "zColumn" ) )
            {
                return geode::Percentage{ 0 };
            }
            return geode::Percentage{ 1 };
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

    } // namespace internal
} // namespace geode