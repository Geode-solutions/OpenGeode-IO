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

#pragma once

#include <fstream>

#include <assimp/scene.h>

#include <geode/io/mesh/common.hpp>

namespace geode
{
    namespace internal
    {
        template < typename Mesh >
        class AssimpMeshInput
        {
        public:
            explicit AssimpMeshInput( std::string_view filename )
                : file_( filename )
            {
                OPENGEODE_EXCEPTION( std::ifstream{ to_string( file_ ) }.good(),
                    "[AssimpMeshInput] Error while opening file: ", file_ );
            }

            virtual ~AssimpMeshInput() = default;

            std::unique_ptr< Mesh > read_file();

        private:
            void read_materials( const aiScene* assimp_scene );

            void read_meshes( const aiScene* assimp_scene );

            void read_textures( const aiScene* assimp_scene );

            std::unique_ptr< Mesh > merge_meshes();

        private:
            std::vector< std::unique_ptr< Mesh > > surfaces_;
            std::string_view file_;
            std::vector< std::pair< std::string, std::string > > materials_;
        };
    } // namespace internal
} // namespace geode
