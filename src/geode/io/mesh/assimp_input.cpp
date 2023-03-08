/*
 * Copyright (c) 2019 - 2023 Geode-solutions
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

#include <geode/io/mesh/private/assimp_input.h>

#include <absl/algorithm/container.h>

#include <geode/basic/common.h>
#include <geode/basic/filename.h>
#include <geode/basic/logger.h>

#include <geode/image/core/raster_image.h>
#include <geode/image/io/raster_image_input.h>

#include <geode/mesh/core/texture2d.h>

namespace
{
    std::vector< geode::Point3D > load_vertices( const aiMesh& mesh )
    {
        std::vector< geode::Point3D > vertices;
        vertices.resize( mesh.mNumVertices );
        for( const auto v : geode::Range{ mesh.mNumVertices } )
        {
            const auto& vertex = mesh.mVertices[v];
            vertices[v] = { { vertex.x, vertex.y, vertex.z } };
        }
        return vertices;
    }

    geode::NNSearch3D::ColocatedInfo build_unique_vertices(
        std::vector< geode::Point3D >&& duplicated_vertices )
    {
        const geode::NNSearch3D colocater{ std::move( duplicated_vertices ) };
        return colocater.colocated_index_mapping( geode::global_epsilon );
    }
} // namespace

namespace geode
{
    namespace detail
    {
        void AssimpMeshInput::read_file()
        {
            const auto* pScene = importer_.ReadFile( to_string( file_ ), 0 );
            OPENGEODE_EXCEPTION( pScene, "[AssimpMeshInput::read_file] ",
                importer_.GetErrorString() );
            assimp_materials_.resize( pScene->mNumMaterials );
            for( const auto i : Range{ pScene->mNumMaterials } )
            {
                const auto material = pScene->mMaterials[i];
                assimp_materials_[i].first = material->GetName().C_Str();
                if( assimp_materials_[i].first.empty() )
                {
                    assimp_materials_[i].first = absl::StrCat( "texture", i );
                }
                if( material->GetTextureCount( aiTextureType_DIFFUSE ) == 0 )
                {
                    continue;
                }
                aiString Path;
                if( material->GetTexture( aiTextureType_DIFFUSE, 0, &Path,
                        nullptr, nullptr, nullptr, nullptr, nullptr )
                    == AI_SUCCESS )
                {
                    assimp_materials_[i].second = absl::StrCat(
                        filepath_without_filename( file_ ), Path.C_Str() );
                }
            }
            assimp_meshes_.resize( pScene->mNumMeshes );
            for( const auto i : Range{ pScene->mNumMeshes } )
            {
                assimp_meshes_[i] = pScene->mMeshes[i];
            }
        }

        void AssimpMeshInput::build_mesh_from_duplicated_vertices()
        {
            for( const auto m : Indices{ assimp_meshes_ } )
            {
                const auto info = build_unique_vertices(
                    load_vertices( *assimp_meshes_[m] ) );
                const auto vertex_offset = build_vertices( info.unique_points );
                const auto polygon_offset =
                    build_polygons( info.colocated_mapping, vertex_offset, m );
                build_texture( polygon_offset, m );
            }
            SurfaceMeshBuilder3D::create( surface_ )
                ->compute_polygon_adjacencies();
        }

        void AssimpMeshInput::build_mesh_without_duplicated_vertices()
        {
            for( const auto m : Indices{ assimp_meshes_ } )
            {
                const auto points = load_vertices( *assimp_meshes_[m] );
                const auto vertex_offset = build_vertices( points );
                absl::FixedArray< index_t > mapping( points.size() );
                absl::c_iota( mapping, 0 );
                const auto polygon_offset =
                    build_polygons( mapping, vertex_offset, m );
                build_texture( polygon_offset, m );
            }
            SurfaceMeshBuilder3D::create( surface_ )
                ->compute_polygon_adjacencies();
        }

        index_t AssimpMeshInput::build_vertices(
            absl::Span< const Point3D > points )
        {
            const auto offset = builder_->create_vertices( points.size() );
            for( const auto v : Indices{ points } )
            {
                builder_->set_point( offset + v, points[v] );
            }
            return offset;
        }

        index_t AssimpMeshInput::build_polygons(
            absl::Span< const index_t > vertex_mapping,
            index_t vertex_offset,
            index_t mesh_id )
        {
            const auto& mesh = *assimp_meshes_[mesh_id];
            const auto offset = surface_.nb_polygons();
            for( const auto p : Range{ mesh.mNumFaces } )
            {
                const auto& face = mesh.mFaces[p];
                absl::FixedArray< index_t > polygon_vertices(
                    face.mNumIndices );
                for( const auto i : LIndices{ polygon_vertices } )
                {
                    polygon_vertices[i] =
                        vertex_offset + vertex_mapping[face.mIndices[i]];
                }
                builder_->create_polygon( polygon_vertices );
            }
            return offset;
        }

        void AssimpMeshInput::build_texture(
            index_t polygon_offset, index_t mesh_id )
        {
            const auto& mesh = *assimp_meshes_[mesh_id];
            if( !mesh.HasTextureCoords( 0 ) )
            {
                return;
            }
            const auto& material = assimp_materials_[mesh.mMaterialIndex];
            auto& texture = surface_.texture_manager().find_or_create_texture(
                material.first );
            for( const auto p : Range{ mesh.mNumFaces } )
            {
                const auto& face = mesh.mFaces[p];
                const auto polygon = polygon_offset + p;
                for( const auto i : LRange{ face.mNumIndices } )
                {
                    const auto mesh_vertex = face.mIndices[i];
                    const auto& coord = mesh.mTextureCoords[0][mesh_vertex];
                    texture.set_texture_coordinates(
                        { polygon, i }, { { coord.x, coord.y } } );
                }
            }
            if( !material.second.empty() )
            {
                try
                {
                    texture.set_image(
                        load_raster_image< 2 >( material.second ) );
                }
                catch( const OpenGeodeException& e )
                {
                    Logger::warn( e.what() );
                }
            }
        }
    } // namespace detail
} // namespace geode
