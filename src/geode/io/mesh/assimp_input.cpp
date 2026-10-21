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

#include <geode/io/mesh/internal/assimp_input.hpp>

#include <assimp/Importer.hpp>

#include <filesystem>
#include <type_traits>

#include <async++.h>

#include <absl/algorithm/container.h>
#include <absl/container/flat_hash_set.h>

#include <geode/basic/common.hpp>
#include <geode/basic/filename.hpp>
#include <geode/basic/logger.hpp>

#include <geode/geometry/point.hpp>

#include <geode/image/core/raster_image.hpp>
#include <geode/image/io/raster_image_input.hpp>

#include <geode/mesh/builder/polygonal_surface_builder.hpp>
#include <geode/mesh/builder/triangulated_surface_builder.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/core/texture2d.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>
#include <geode/mesh/helpers/detail/surface_merger.hpp>

namespace
{
    template < typename Mesh >
    bool is_face_supported( const aiFace& face )
    {
        if constexpr( std::is_same_v< Mesh, geode::TriangulatedSurface3D > )
        {
            return face.mNumIndices == 3;
        }
        return face.mNumIndices >= 3 && face.mNumIndices < geode::NO_LID;
    }

    template < typename Mesh >
    std::unique_ptr< Mesh > build_mesh( const aiMesh& assimp_mesh,
        std::string_view texture_name,
        geode::index_t& nb_skipped_faces )
    {
        auto mesh = Mesh::create();
        auto builder = Mesh::Builder::create( *mesh );
        std::vector< geode::index_t > new_vertices(
            assimp_mesh.mNumVertices, geode::NO_ID );
        geode::index_t nb_vertices{ 0 };
        bool has_shared_vertices{ false };
        for( const auto p : geode::Range{ assimp_mesh.mNumFaces } )
        {
            const auto& face = assimp_mesh.mFaces[p];
            if( !is_face_supported< Mesh >( face ) )
            {
                nb_skipped_faces++;
                continue;
            }
            for( const auto i : geode::Range{ face.mNumIndices } )
            {
                auto& new_vertex = new_vertices[face.mIndices[i]];
                if( new_vertex == geode::NO_ID )
                {
                    new_vertex = nb_vertices++;
                }
                else
                {
                    has_shared_vertices = true;
                }
            }
        }
        builder->create_vertices( nb_vertices );
        async::parallel_for(
            async::irange( geode::index_t{ 0 }, assimp_mesh.mNumVertices ),
            [&assimp_mesh, &builder, &new_vertices]( geode::index_t v ) {
                if( new_vertices[v] == geode::NO_ID )
                {
                    return;
                }
                const auto& vertex = assimp_mesh.mVertices[v];
                builder->set_point( new_vertices[v],
                    geode::Point3D{ { vertex.x, vertex.y, vertex.z } } );
            } );
        geode::Texture2D* texture{ nullptr };
        if( assimp_mesh.HasTextureCoords( 0 ) )
        {
            texture =
                &mesh->texture_manager().find_or_create_texture( texture_name );
        }
        for( const auto p : geode::Range{ assimp_mesh.mNumFaces } )
        {
            const auto& face = assimp_mesh.mFaces[p];
            if( !is_face_supported< Mesh >( face ) )
            {
                continue;
            }
            absl::FixedArray< geode::index_t > polygon_vertices(
                face.mNumIndices );
            for( const auto i : geode::LIndices{ polygon_vertices } )
            {
                polygon_vertices[i] = new_vertices[face.mIndices[i]];
            }
            const auto polygon = builder->create_polygon( polygon_vertices );
            if( !texture )
            {
                continue;
            }
            for( const auto i : geode::LIndices{ polygon_vertices } )
            {
                const auto& coord =
                    assimp_mesh.mTextureCoords[0][face.mIndices[i]];
                texture->set_texture_coordinates(
                    { polygon, i }, geode::Point2D{ { coord.x, coord.y } } );
            }
        }
        // Without shared vertices (e.g. OBJ, STL or DXF importers) there is no
        // adjacency to compute
        if( has_shared_vertices )
        {
            builder->compute_polygon_adjacencies();
        }
        return mesh;
    }
} // namespace

namespace geode
{
    namespace internal
    {
        template < typename Mesh >
        Percentage AssimpMeshInput< Mesh >::is_loadable()
        {
            Assimp::Importer importer;
            const auto* assimp_scene =
                importer.ReadFile( to_string( file_ ), 0 );
            return Percentage{ assimp_scene ? 1. : 0. };
        }

        template < typename Mesh >
        std::unique_ptr< Mesh > AssimpMeshInput< Mesh >::read_file()
        {
            Assimp::Importer importer;
            const auto* assimp_scene =
                importer.ReadFile( to_string( file_ ), 0 );
            OpenGeodeIOMeshException::check_exception( assimp_scene, nullptr,
                OpenGeodeException::TYPE::data, "[AssimpMeshInput::read_file] ",
                importer.GetErrorString() );
            read_materials( assimp_scene );
            read_meshes( assimp_scene );
            importer.FreeScene();
            return merge_meshes();
        }

        template < typename Mesh >
        void AssimpMeshInput< Mesh >::read_meshes( const aiScene* assimp_scene )
        {
            surfaces_.resize( assimp_scene->mNumMeshes );
            absl::FixedArray< async::task< void > > tasks(
                assimp_scene->mNumMeshes );
            absl::FixedArray< index_t > nb_skipped_faces(
                assimp_scene->mNumMeshes, 0 );
            for( const auto i : Range{ assimp_scene->mNumMeshes } )
            {
                tasks[i] =
                    async::spawn( [this, i, &assimp_scene, &nb_skipped_faces] {
                        const auto& assimp_mesh = *assimp_scene->mMeshes[i];
                        surfaces_[i] = build_mesh< Mesh >( assimp_mesh,
                            materials_[assimp_mesh.mMaterialIndex].first,
                            nb_skipped_faces[i] );
                    } );
            }
            for( auto& task :
                async::when_all( tasks.begin(), tasks.end() ).get() )
            {
                task.get();
            }
            index_t total_skipped_faces{ 0 };
            for( const auto nb : nb_skipped_faces )
            {
                total_skipped_faces += nb;
            }
            if( total_skipped_faces > 0 )
            {
                Logger::warning( "[AssimpMeshInput::read_meshes] Skipped ",
                    total_skipped_faces, " unsupported faces (",
                    std::is_same_v< Mesh, TriangulatedSurface3D >
                        ? "non-triangle faces"
                        : "lines, points, empty faces or polygons with too "
                          "many vertices",
                    ") in file ", file_ );
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

            detail::SurfaceMeshMerger3D merger{ ref_surfaces };

            std::unique_ptr< Mesh > merged{ dynamic_cast< Mesh* >(
                merger.merge( GLOBAL_EPSILON ).release() ) };
            Mesh::Builder::create( *merged )->compute_polygon_adjacencies();
            auto merged_manager = merged->texture_manager();
            for( const auto s : Indices{ surfaces_ } )
            {
                const auto& mesh = surfaces_[s];
                auto manager = mesh->texture_manager();
                for( const auto& name : manager.texture_names() )
                {
                    const auto& texture = manager.find_texture( name );
                    auto& merged_texture =
                        merged_manager.find_or_create_texture( name );
                    for( const auto p : Range{ mesh->nb_polygons() } )
                    {
                        const auto merged_polygon =
                            merger.polygon_in_merged( s, p );
                        if( merged_polygon == NO_ID )
                        {
                            continue;
                        }
                        // The merged polygon may start on another vertex or
                        // be reversed: match corners by their vertex
                        for( const auto v :
                            LRange{ mesh->nb_polygon_vertices( p ) } )
                        {
                            const auto merged_vertex = merger.vertex_in_merged(
                                s, mesh->polygon_vertex( { p, v } ) );
                            const auto merged_v = merged->vertex_in_polygon(
                                merged_polygon, merged_vertex );
                            if( !merged_v )
                            {
                                continue;
                            }
                            merged_texture.set_texture_coordinates(
                                { merged_polygon, merged_v.value() },
                                texture.texture_coordinates( { p, v } ) );
                        }
                    }
                }
            }
            // Load each texture image once, directly in the merged mesh
            absl::flat_hash_set< std::string_view > loaded_textures;
            for( const auto& [name, image_path] : materials_ )
            {
                if( image_path.empty() || !merged_manager.texture_exists( name )
                    || !loaded_textures.emplace( name ).second )
                {
                    continue;
                }
                try
                {
                    merged_manager.find_or_create_texture( name ).set_image(
                        load_raster_image< 2 >( image_path ) );
                }
                catch( const OpenGeodeException& e )
                {
                    Logger::warning( e.what() );
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
                    std::filesystem::path texture_path{ Path.C_Str() };
                    if( texture_path.is_relative() )
                    {
                        texture_path =
                            filepath_without_filename( file_ ) / texture_path;
                    }
                    materials_[i].second = texture_path.string();
                }
            }
        }

        template class AssimpMeshInput< PolygonalSurface3D >;
        template class AssimpMeshInput< TriangulatedSurface3D >;
    } // namespace internal
} // namespace geode
