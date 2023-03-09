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

#pragma once

#include <geode/io/mesh/private/vtk_mesh_output.h>

#include <geode/basic/filename.h>

#include <geode/geometry/bounding_box.h>

#include <geode/image/core/raster_image.h>
#include <geode/image/io/raster_image_output.h>

#include <geode/mesh/core/texture2d.h>

namespace geode
{
    namespace detail
    {
        template < template < index_t > class Mesh, index_t dimension >
        class VTPSurfaceOutputImpl : public VTKMeshOutputImpl< Mesh, dimension >
        {
        public:
            VTPSurfaceOutputImpl( absl::string_view filename,
                const Mesh< dimension >& polygonal_surface )
                : VTKMeshOutputImpl< Mesh, dimension >(
                    filename, polygonal_surface, "PolyData" )
            {
                const auto manager = this->mesh().texture_manager();
                const auto texture_names = manager.texture_names();
                textures_info_.reserve( texture_names.size() );
                for( const auto texture_name : texture_names )
                {
                    textures_info_.emplace_back(
                        texture_name, manager.find_texture( texture_name ) );
                }
                if( !textures_info_.empty() )
                {
                    vertex_mapping_.resize( polygonal_surface.nb_vertices() );
                }
            }

        private:
            void append_number_elements( pugi::xml_node& piece ) override
            {
                piece.append_attribute( "NumberOfPolys" )
                    .set_value( this->mesh().nb_polygons() );
            }

            void save_images() const
            {
                const auto path =
                    filepath_without_extension( this->filename() );
                for( const auto& texture : textures_info_ )
                {
                    const auto& image = texture.second.get().image();
                    if( image.nb_cells() > 0 )
                    {
                        save_raster_image( image,
                            absl::StrCat( path, "_", texture.first, ".vti" ) );
                    }
                }
            }

            void write_vtk_textures( pugi::xml_node& vertex_node ) override
            {
                if( textures_info_.empty() )
                {
                    return;
                }
                save_images();
                for( const auto& texture_info : textures_info_ )
                {
                    auto data_array = this->write_attribute_header(
                        vertex_node, texture_info.first, 2 );
                    BoundingBox2D bbox;
                    std::string values;
                    const auto& texture = texture_info.second.get();
                    for( const auto& texture_vertex : unique_texture_vertices_ )
                    {
                        const auto& coordinates =
                            texture.texture_coordinates( texture_vertex );
                        absl::StrAppend( &values, coordinates.string(), " " );
                        bbox.add_point( coordinates );
                    }
                    const auto min = std::min(
                        bbox.min().value( 0 ), bbox.min().value( 1 ) );
                    const auto max = std::max(
                        bbox.max().value( 0 ), bbox.max().value( 1 ) );
                    data_array.append_attribute( "RangeMin" ).set_value( min );
                    data_array.append_attribute( "RangeMax" ).set_value( max );
                    data_array.text().set( values.c_str() );
                }
            }

            std::vector< index_t > compute_vertices() override
            {
                if( textures_info_.empty() )
                {
                    return VTKMeshOutputImpl< Mesh,
                        dimension >::compute_vertices();
                }
                const auto& mesh = this->mesh();
                std::vector< index_t > vertices;
                vertices.reserve( mesh.nb_vertices() );
                using TextureCoords = absl::InlinedVector< Point2D, 1 >;
                absl::FixedArray<
                    absl::flat_hash_map< TextureCoords, index_t > >
                    vertices_textures( mesh.nb_vertices() );
                for( const auto p : Range{ mesh.nb_polygons() } )
                {
                    for( const auto v :
                        LRange{ mesh.nb_polygon_vertices( p ) } )
                    {
                        const PolygonVertex pv{ p, v };
                        const auto vertex = mesh.polygon_vertex( pv );
                        auto& vertex_textures = vertices_textures[vertex];
                        auto& mapping = vertex_mapping_[vertex];
                        TextureCoords coords;
                        coords.reserve( textures_info_.size() );
                        for( const auto& texture : textures_info_ )
                        {
                            coords.emplace_back(
                                texture.second.get().texture_coordinates(
                                    pv ) );
                        }
                        const auto it = vertex_textures.try_emplace(
                            std::move( coords ), vertices.size() );
                        if( it.second )
                        {
                            vertices.push_back( vertex );
                            unique_texture_vertices_.push_back( pv );
                        }
                        mapping[p] = it.first->second;
                    }
                }
                return vertices;
            }

            pugi::xml_node write_vtk_cells( pugi::xml_node& piece ) override
            {
                auto polys = piece.append_child( "Polys" );
                auto connectivity = polys.append_child( "DataArray" );
                connectivity.append_attribute( "type" ).set_value( "Int64" );
                connectivity.append_attribute( "Name" ).set_value(
                    "connectivity" );
                connectivity.append_attribute( "format" ).set_value( "ascii" );
                connectivity.append_attribute( "RangeMin" ).set_value( 0 );
                connectivity.append_attribute( "RangeMax" )
                    .set_value( this->mesh().nb_vertices() - 1 );
                auto offsets = polys.append_child( "DataArray" );
                offsets.append_attribute( "type" ).set_value( "Int64" );
                offsets.append_attribute( "Name" ).set_value( "offsets" );
                offsets.append_attribute( "format" ).set_value( "ascii" );
                offsets.append_attribute( "RangeMin" ).set_value( 0 );
                offsets.append_attribute( "RangeMax" )
                    .set_value( this->mesh().nb_vertices() );
                const auto nb_polys = this->mesh().nb_polygons();
                std::string poly_connectivity;
                poly_connectivity.reserve( nb_polys * 3 );
                std::string poly_offsets;
                poly_offsets.reserve( nb_polys );
                index_t vertex_count{ 0 };
                for( const auto p : Range{ nb_polys } )
                {
                    const auto nb_polygon_vertices =
                        this->mesh().nb_polygon_vertices( p );
                    vertex_count += nb_polygon_vertices;
                    absl::StrAppend( &poly_offsets, vertex_count, " " );
                    for( const auto v : LRange{ nb_polygon_vertices } )
                    {
                        const auto vertex =
                            this->mesh().polygon_vertex( { p, v } );
                        if( vertex_mapping_.empty() )
                        {
                            absl::StrAppend( &poly_connectivity, vertex, " " );
                        }
                        else
                        {
                            absl::StrAppend( &poly_connectivity,
                                vertex_mapping_[vertex].at( p ), " " );
                        }
                    }
                }
                connectivity.text().set( poly_connectivity.c_str() );
                offsets.text().set( poly_offsets.c_str() );
                return polys;
            }

            pugi::xml_node write_vtk_cell_attributes(
                pugi::xml_node& piece ) override
            {
                auto cell_data = piece.append_child( "CellData" );
                this->write_attributes(
                    cell_data, this->mesh().polygon_attribute_manager() );
                return cell_data;
            }

        private:
            std::vector< std::pair< absl::string_view,
                std::reference_wrapper< const Texture2D > > >
                textures_info_;
            std::vector< absl::flat_hash_map< index_t, index_t > >
                vertex_mapping_;
            std::vector< PolygonVertex > unique_texture_vertices_;
        };
    } // namespace detail
} // namespace geode
