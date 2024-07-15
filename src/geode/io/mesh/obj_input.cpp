/*
 * Copyright (c) 2019 - 2024 Geode-solutions
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

#include <geode/io/mesh/internal/obj_input.h>

#include <fstream>

#include <geode/basic/file.h>
#include <geode/basic/filename.h>
#include <geode/basic/string.h>

#include <geode/mesh/core/polygonal_surface.h>

#include <geode/io/mesh/internal/assimp_input.h>

namespace geode
{
    namespace internal
    {
        std::unique_ptr< PolygonalSurface3D > OBJInput::read(
            const MeshImpl& /*unused*/ )
        {
            geode::internal::AssimpMeshInput< geode::PolygonalSurface3D >
                reader{ filename() };
            return reader.read_file();
        }

        PolygonalSurfaceInput< 3 >::MissingFiles
            OBJInput::check_missing_files() const
        {
            std::ifstream obj_file{ to_string( filename() ) };
            OPENGEODE_EXCEPTION( obj_file,
                "[OBJInput::check_missing_files] Failed to open file: ",
                filename() );
            const auto mtllib_line =
                goto_keyword_if_it_exists( obj_file, "mtllib" );
            if( !mtllib_line )
            {
                return {};
            }
            PolygonalSurfaceInput< 3 >::MissingFiles missing;
            const auto mtl_tokens = string_split( mtllib_line.value() );
            const auto& mtl_filename = mtl_tokens.at( 1 );
            const auto file_path =
                filepath_without_filename( filename() ).string();
            const auto mtl_file_path =
                absl::StrCat( file_path, "/", mtl_filename );
            if( !file_exists( mtl_file_path ) )
            {
                missing.additional_files.emplace_back( mtl_filename );
                return missing;
            }

            std::ifstream mtl_file{ mtl_file_path };
            OPENGEODE_EXCEPTION( mtl_file,
                "[OBJInput::check_missing_files] Failed to open file: ",
                mtl_file_path );
            while( const auto texture_line =
                       goto_keyword_if_it_exists( mtl_file, "map_Kd" ) )
            {
                const auto texture_tokens =
                    string_split( texture_line.value() );
                const auto& texture_filename = texture_tokens.at( 1 );
                const auto texture_file_path =
                    absl::StrCat( file_path, "/", texture_filename );
                if( !file_exists( texture_file_path ) )
                {
                    missing.additional_files.emplace_back( texture_filename );
                }
            }
            return missing;
        }
    } // namespace internal
} // namespace geode
