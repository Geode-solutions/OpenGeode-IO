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

#include <assimp/Importer.hpp>

#include <async++.h>

#include <absl/algorithm/container.h>

#include <geode/basic/common.h>
#include <geode/basic/filename.h>
#include <geode/basic/logger.h>

#include <geode/geometry/point.h>

#include <geode/image/core/raster_image.h>
#include <geode/image/io/raster_image_input.h>

#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/builder/triangulated_surface_builder.h>
#include <geode/mesh/core/polygonal_surface.h>
#include <geode/mesh/core/texture2d.h>
#include <geode/mesh/core/triangulated_surface.h>
#include <geode/mesh/helpers/detail/surface_merger.h>

namespace
{
    template < typename Mesh >
    std::unique_ptr< Mesh > build_mesh( const aiMesh& assimp_mesh )
    {
        auto mesh = Mesh::create();
        auto builder = Mesh::Builder::create( *mesh );
        builder->create_vertices( assimp_mesh.mNumVertices );
        async::parallel_for(
            async::irange( geode::index_t{ 0 }, assimp_mesh.mNumVertices ),
            [&assimp_mesh, &builder]( geode::index_t v ) {
                const auto& vertex = assimp_mesh.mVertices[v];
                builder->set_point( v, { { vertex.x, vertex.y, vertex.z } } );
            } );
        for( const auto p : geode::Range{ assimp_mesh.mNumFaces } )
        {
            const auto& face = assimp_mesh.mFaces[p];
            absl::FixedArray< geode::index_t > polygon_vertices(
                face.mNumIndices );
            for( const auto i : geode::LIndices{ polygon_vertices } )
            {
                polygon_vertices[i] = face.mIndices[i];
            }
            builder->create_polygon( polygon_vertices );
        }
        builder->compute_polygon_adjacencies();
        return mesh;
    }
} // namespace

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        std::unique_ptr< Mesh > AssimpMeshInput< Mesh >::read_file()
        {
            Assimp::Importer importer;
            const auto* assimp_scene =
                importer.ReadFile( to_string( file_ ), 0 );
            OPENGEODE_EXCEPTION( assimp_scene, "[AssimpMeshInput::read_file] ",
                importer.GetErrorString() );
            read_materials( assimp_scene );
            read_meshes( assimp_scene );
            read_textures( assimp_scene );
            return merge_meshes();
        }

        template < typename Mesh >
        void AssimpMeshInput< Mesh >::read_textures(
            const aiScene* assimp_scene )
        {
            for( const auto i : Range{ assimp_scene->mNumMeshes } )
            {
                const auto& assimp_mesh = *assimp_scene->mMeshes[i];
                if( !assimp_mesh.HasTextureCoords( 0 ) )
                {
                    return;
                }
                const auto& mesh = *surfaces_[i];
                const auto& material = materials_[assimp_mesh.mMaterialIndex];
                auto& texture = mesh.texture_manager().find_or_create_texture(
                    material.first );
                for( const auto p : Range{ assimp_mesh.mNumFaces } )
                {
                    const auto& face = assimp_mesh.mFaces[p];
                    for( const auto v : LRange{ face.mNumIndices } )
                    {
                        const auto mesh_vertex = face.mIndices[v];
                        const auto& coord =
                            assimp_mesh.mTextureCoords[0][mesh_vertex];
                        texture.set_texture_coordinates(
                            { p, v }, { { coord.x, coord.y } } );
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
        }

        template < typename Mesh >
        void AssimpMeshInput< Mesh >::read_meshes( const aiScene* assimp_scene )
        {
            surfaces_.resize( assimp_scene->mNumMeshes );
            absl::FixedArray< async::task< void > > tasks(
                assimp_scene->mNumMeshes );
            for( const auto i : Range{ assimp_scene->mNumMeshes } )
            {
                tasks[i] = async::spawn( [this, i, &assimp_scene] {
                    surfaces_[i] =
                        build_mesh< Mesh >( *assimp_scene->mMeshes[i] );
                } );
            }
            for( auto& task :
                async::when_all( tasks.begin(), tasks.end() ).get() )
            {
                task.get();
            }
        }

        template < typename Mesh >
        std::unique_ptr< Mesh > AssimpMeshInput< Mesh >::merge_meshes()
        {
            std::vector< std::reference_wrapper< const SurfaceMesh3D > >
                ref_surfaces;
            ref_surfaces.reserve( surfaces_.size() );
            for( const auto& surface : surfaces_ )
            {
                ref_surfaces.emplace_back( *surface );
            }
            SurfaceMeshMerger3D merger{ ref_surfaces, global_epsilon };
            std::unique_ptr< Mesh > merged{ dynamic_cast< Mesh* >(
                merger.merge().release() ) };
            Mesh::Builder::create( *merged )->compute_polygon_adjacencies();
            auto merged_manager = merged->texture_manager();
            for( const auto s : Indices{ surfaces_ } )
            {
                const auto& mesh = surfaces_[s];
                auto manager = mesh->texture_manager();
                for( const auto name : manager.texture_names() )
                {
                    const auto& texture = manager.find_texture( name );
                    auto& merged_texture =
                        merged_manager.find_or_create_texture( name );
                    merged_texture.set_image( texture.image().clone() );
                    for( const auto p : Range{ mesh->nb_polygons() } )
                    {
                        const auto merged_polygon =
                            merger.polygon_in_merged( s, p );
                        if( merged_polygon == NO_ID )
                        {
                            continue;
                        }
                        for( const auto v :
                            LRange{ mesh->nb_polygon_vertices( p ) } )
                        {
                            merged_texture.set_texture_coordinates(
                                { merged_polygon, v },
                                texture.texture_coordinates( { p, v } ) );
                        }
                    }
                }
            }
            return merged;
        }

        template < typename Mesh >
        void AssimpMeshInput< Mesh >::read_materials(
            const aiScene* assimp_scene )
        {
            materials_.resize( assimp_scene->mNumMaterials );
            for( const auto i : Range{ assimp_scene->mNumMaterials } )
            {
                const auto material = assimp_scene->mMaterials[i];
                materials_[i].first = material->GetName().C_Str();
                if( materials_[i].first.empty() )
                {
                    materials_[i].first = absl::StrCat( "texture", i );
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
                    materials_[i].second = absl::StrCat(
                        filepath_without_filename( file_ ), Path.C_Str() );
                }
            }
        }

        template class AssimpMeshInput< PolygonalSurface3D >;
        template class AssimpMeshInput< TriangulatedSurface3D >;
    } // namespace detail
} // namespace geode
