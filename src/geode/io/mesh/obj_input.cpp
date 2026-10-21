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

#include <geode/io/mesh/internal/obj_input.hpp>

#include <filesystem>
#include <fstream>

#include <absl/strings/ascii.h>
#include <absl/strings/match.h>
#include <absl/strings/numbers.h>

#include <geode/basic/file.hpp>
#include <geode/basic/filename.hpp>
#include <geode/basic/logger.hpp>

#include <geode/mesh/core/polygonal_surface.hpp>

#include <geode/io/mesh/internal/assimp_input.hpp>

namespace
{
    std::string_view keyword_value(
        std::string_view line, std::string_view keyword )
    {
        return absl::StripAsciiWhitespace(
            absl::StripAsciiWhitespace( line ).substr( keyword.size() ) );
    }

    bool is_option_value( std::string_view token )
    {
        double value;
        return absl::SimpleAtod( token, &value ) || token == "on"
               || token == "off";
    }

    // Removes texture options (e.g. "-clamp on -s 1 1 1 texture.png")
    // to keep only the texture filename, which may contain spaces
    std::string_view skip_texture_options( std::string_view value )
    {
        while( absl::StartsWith( value, "-" ) )
        {
            const auto option_end = value.find_first_of( " \t" );
            if( option_end == std::string_view::npos )
            {
                return {};
            }
            const auto option = value.substr( 0, option_end );
            value =
                absl::StripLeadingAsciiWhitespace( value.substr( option_end ) );
            const auto single_word_option =
                option == "-type" || option == "-imfchan";
            while( !value.empty() )
            {
                const auto token_end = value.find_first_of( " \t" );
                if( token_end == std::string_view::npos )
                {
                    // last token is the texture filename
                    break;
                }
                const auto token = value.substr( 0, token_end );
                if( !single_word_option && !is_option_value( token ) )
                {
                    break;
                }
                value = absl::StripLeadingAsciiWhitespace(
                    value.substr( token_end ) );
                if( single_word_option )
                {
                    break;
                }
            }
        }
        return value;
    }

    std::filesystem::path resolve_path(
        const std::filesystem::path& directory, std::string_view filename )
    {
        std::filesystem::path path{ filename };
        if( path.is_relative() )
        {
            return directory / path;
        }
        return path;
    }
} // namespace

namespace geode
{
    namespace internal
    {
        std::unique_ptr< PolygonalSurface3D > OBJInput::read(
            const MeshImpl& /*unused*/ )
        {
            AssimpMeshInput< PolygonalSurface3D > reader{ filename() };
            return reader.read_file();
        }

        auto OBJInput::additional_files() const -> AdditionalFiles
        {
            std::ifstream obj_file{ to_string( filename() ) };
            OpenGeodeIOMeshException::check_exception( !obj_file.fail(),
                nullptr, OpenGeodeException::TYPE::data,
                "[OBJInput::additional_files] Failed to open file: ",
                filename() );
            const auto mtllib_line =
                goto_keyword_if_it_exists( obj_file, "mtllib" );
            if( !mtllib_line )
            {
                return {};
            }
            const auto mtl_filename =
                keyword_value( mtllib_line.value(), "mtllib" );
            if( mtl_filename.empty() )
            {
                Logger::warning( "[OBJInput::additional_files] Missing "
                                 "material library name in file ",
                    filename() );
                return {};
            }
            AdditionalFiles files;
            const auto file_path = filepath_without_filename( filename() );
            const auto mtl_file_path = resolve_path( file_path, mtl_filename );
            const auto mtl_is_missing =
                !std::filesystem::is_regular_file( mtl_file_path );
            files.optional_files.emplace_back(
                to_string( mtl_filename ), mtl_is_missing );
            if( mtl_is_missing )
            {
                return files;
            }

            std::ifstream mtl_file{ mtl_file_path };
            OpenGeodeIOMeshException::check_exception( !mtl_file.fail(),
                nullptr, OpenGeodeException::TYPE::data,
                "[OBJInput::additional_files] Failed to open file: ",
                mtl_file_path.string() );
            while( const auto texture_line =
                       goto_keyword_if_it_exists( mtl_file, "map_Kd" ) )
            {
                const auto texture_filename = skip_texture_options(
                    keyword_value( texture_line.value(), "map_Kd" ) );
                if( texture_filename.empty() )
                {
                    Logger::warning( "[OBJInput::additional_files] Missing "
                                     "texture name in file ",
                        mtl_file_path.string() );
                    continue;
                }
                const auto texture_is_missing =
                    !std::filesystem::is_regular_file(
                        resolve_path( file_path, texture_filename ) );
                files.optional_files.emplace_back(
                    to_string( texture_filename ), texture_is_missing );
            }
            return files;
        }

        Percentage OBJInput::is_loadable() const
        {
            AssimpMeshInput< PolygonalSurface3D > reader{ filename() };
            return reader.is_loadable();
        }
    } // namespace internal
} // namespace geode
