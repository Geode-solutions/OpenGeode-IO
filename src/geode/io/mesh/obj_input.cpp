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

#include <geode/io/mesh/internal/obj_input.hpp>

#include <fstream>

#include <geode/basic/file.hpp>
#include <geode/basic/filename.hpp>
#include <geode/basic/string.hpp>

#include <geode/mesh/core/polygonal_surface.hpp>

#include <geode/io/mesh/internal/assimp_input.hpp>

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
            OPENGEODE_EXCEPTION( obj_file,
                "[OBJInput::additional_files] Failed to open file: ",
                filename() );
            const auto mtllib_line =
                goto_keyword_if_it_exists( obj_file, "mtllib" );
            if( !mtllib_line )
            {
                return {};
            }
            AdditionalFiles files;
            const auto mtl_tokens = string_split( mtllib_line.value() );
            const auto& mtl_filename = mtl_tokens.at( 1 );
            const auto file_path =
                filepath_without_filename( filename() ).string();
            const auto mtl_file_path =
                absl::StrCat( file_path, "/", mtl_filename );
            const auto mtl_is_missing = !file_exists( mtl_file_path );
            files.optional_files.emplace_back(
                to_string( mtl_filename ), mtl_is_missing );
            if( mtl_is_missing )
            {
                return files;
            }

            std::ifstream mtl_file{ mtl_file_path };
            OPENGEODE_EXCEPTION( mtl_file,
                "[OBJInput::additional_files] Failed to open file: ",
                mtl_file_path );
            while( const auto texture_line =
                       goto_keyword_if_it_exists( mtl_file, "map_Kd" ) )
            {
                const auto texture_tokens =
                    string_split( texture_line.value() );
                const auto& texture_filename = texture_tokens.at( 1 );
                const auto texture_file_path =
                    absl::StrCat( file_path, "/", texture_filename );
                const auto texture_is_missing =
                    !file_exists( texture_file_path );
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
