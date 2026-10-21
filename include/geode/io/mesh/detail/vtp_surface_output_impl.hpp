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

#include <geode/io/mesh/detail/vtk_mesh_output.hpp>

#include <absl/strings/ascii.h>

#include <geode/basic/filename.hpp>

#include <geode/image/core/raster_image.hpp>
#include <geode/image/io/raster_image_output.hpp>

#include <geode/mesh/core/texture2d.hpp>

namespace geode
{
    namespace detail
    {
        template < template < index_t > class Mesh, index_t dimension >
        class VTPSurfaceOutputImpl : public VTKMeshOutputImpl< Mesh, dimension >
        {
        public:
            VTPSurfaceOutputImpl( std::string_view filename,
                const Mesh< dimension >& polygonal_surface )
                : VTKMeshOutputImpl< Mesh, dimension >(
                      filename, polygonal_surface, "PolyData" )
            {
                const auto manager = this->mesh().texture_manager();
                const auto texture_names = manager.texture_names();
                textures_info_.reserve( texture_names.size() );
                for( const auto& texture_name : texture_names )
                {
                    textures_info_.emplace_back(
                        texture_name, manager.find_texture( texture_name ) );
                }
            }

            // Files written by write_file(): the VTP file then the texture
            // images
            std::vector< std::string > written_files() const
            {
                std::vector< std::string > files{ to_string(
                    this->filename() ) };
                files.insert(
                    files.end(), image_files_.begin(), image_files_.end() );
                return files;
            }

        private:
            void append_number_elements( pugi::xml_node& piece ) override
            {
                piece.append_attribute( "NumberOfPolys" )
                    .set_value( this->mesh().nb_polygons() );
            }

            // Texture names are free strings, keep only characters safe
            // for a file name
            static std::string texture_file_suffix( std::string_view name )
            {
                std::string suffix{ name };
                for( auto& character : suffix )
                {
                    if( !absl::ascii_isalnum(
                            static_cast< unsigned char >( character ) )
                        && character != '-' && character != '_'
                        && character != '.' )
                    {
                        character = '_';
                    }
                }
                return suffix;
            }

            void save_images()
            {
                const auto path =
                    filepath_without_extension( this->filename() ).string();
                for( const auto& texture : textures_info_ )
                {
                    const auto& image = texture.second.get().image();
                    if( image.nb_cells() > 0 )
                    {
                        auto files = save_raster_image(
                            image, absl::StrCat( path, "_",
                                       texture_file_suffix( texture.first ),
                                       ".vti" ) );
                        image_files_.insert( image_files_.end(),
                            std::make_move_iterator( files.begin() ),
                            std::make_move_iterator( files.end() ) );
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
                    std::vector< double > values;
                    values.reserve( 2 * unique_texture_vertices_.size() );
                    const auto& texture = texture_info.second.get();
                    for( const auto& texture_vertex : unique_texture_vertices_ )
                    {
                        const auto& coordinates =
                            texture.texture_coordinates( texture_vertex );
                        values.push_back( coordinates.value( 0 ) );
                        values.push_back( coordinates.value( 1 ) );
                    }
                    this->template write_data_array< double >( vertex_node,
                        texture_info.first, std::move( values ), 2 );
                }
            }

            std::vector< index_t > compute_vertices() override
            {
                if( textures_info_.empty() )
                {
                    return VTKMeshOutputImpl< Mesh,
                        dimension >::compute_vertices();
                }
                // A vertex is split into one output vertex per distinct set
                // of texture coordinates. Output vertices of a same mesh
                // vertex are chained to find an existing one.
                const auto& mesh = this->mesh();
                std::vector< index_t > vertices;
                vertices.reserve( mesh.nb_vertices() );
                std::vector< index_t > first_output_vertex(
                    mesh.nb_vertices(), NO_ID );
                std::vector< index_t > next_output_vertex;
                next_output_vertex.reserve( mesh.nb_vertices() );
                for( const auto p : Range{ mesh.nb_polygons() } )
                {
                    for( const auto v :
                        LRange{ mesh.nb_polygon_vertices( p ) } )
                    {
                        const PolygonVertex pv{ p, v };
                        const auto vertex = mesh.polygon_vertex( pv );
                        auto output = first_output_vertex[vertex];
                        auto previous = NO_ID;
                        while( output != NO_ID
                               && !same_texture_coordinates(
                                   pv, unique_texture_vertices_[output] ) )
                        {
                            previous = output;
                            output = next_output_vertex[output];
                        }
                        if( output == NO_ID )
                        {
                            output = vertices.size();
                            vertices.push_back( vertex );
                            unique_texture_vertices_.push_back( pv );
                            next_output_vertex.push_back( NO_ID );
                            if( previous == NO_ID )
                            {
                                first_output_vertex[vertex] = output;
                            }
                            else
                            {
                                next_output_vertex[previous] = output;
                            }
                        }
                        corner_output_vertices_.push_back( output );
                    }
                }
                return vertices;
            }

            bool same_texture_coordinates(
                const PolygonVertex& pv0, const PolygonVertex& pv1 ) const
            {
                for( const auto& texture : textures_info_ )
                {
                    if( texture.second.get().texture_coordinates( pv0 )
                        != texture.second.get().texture_coordinates( pv1 ) )
                    {
                        return false;
                    }
                }
                return true;
            }

            pugi::xml_node write_vtk_cells( pugi::xml_node& piece ) override
            {
                auto polys = piece.append_child( "Polys" );
                const auto nb_polys = this->mesh().nb_polygons();
                std::vector< int64_t > poly_connectivity;
                poly_connectivity.reserve(
                    static_cast< size_t >( nb_polys ) * 3 );
                std::vector< int64_t > poly_offsets;
                poly_offsets.reserve( nb_polys );
                // 64-bit accumulator: offsets may exceed index_t range
                int64_t vertex_count{ 0 };
                index_t corner{ 0 };
                for( const auto p : Range{ nb_polys } )
                {
                    const auto nb_polygon_vertices =
                        this->mesh().nb_polygon_vertices( p );
                    vertex_count += nb_polygon_vertices;
                    poly_offsets.push_back( vertex_count );
                    for( const auto v : LRange{ nb_polygon_vertices } )
                    {
                        if( corner_output_vertices_.empty() )
                        {
                            poly_connectivity.push_back(
                                this->mesh().polygon_vertex( { p, v } ) );
                        }
                        else
                        {
                            poly_connectivity.push_back(
                                corner_output_vertices_[corner++] );
                        }
                    }
                }
                this->template write_data_array< int64_t >(
                    polys, "connectivity", std::move( poly_connectivity ) );
                this->template write_data_array< int64_t >(
                    polys, "offsets", std::move( poly_offsets ) );
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
            std::vector< std::pair< std::string,
                std::reference_wrapper< const Texture2D > > >
                textures_info_;
            // Output vertex of each polygon corner, in polygon order
            std::vector< index_t > corner_output_vertices_;
            std::vector< PolygonVertex > unique_texture_vertices_;
            std::vector< std::string > image_files_;
        };
    } // namespace detail
} // namespace geode
