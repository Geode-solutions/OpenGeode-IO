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

#include <geode/io/mesh/common.hpp>

#include <fstream>

#include <pugixml.hpp>

#include <zlib.h>

#include <absl/strings/escaping.h>
#include <absl/strings/str_split.h>

#include <geode/basic/attribute_manager.hpp>

#include <geode/geometry/point.hpp>

#include <geode/io/mesh/detail/vtk_input.hpp>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class VTKMeshInputImpl : public VTKInputImpl< Mesh >
        {
            using MeshBuilder = typename Mesh::Builder;

        protected:
            VTKMeshInputImpl( std::string_view filename,
                const MeshImpl& impl,
                const char* type )
                : VTKInputImpl< Mesh >{ filename, type }
            {
                this->initialize_mesh( Mesh::create( impl ) );
                mesh_builder_ = MeshBuilder::create( this->mesh() );
            }

            MeshBuilder& builder()
            {
                return *mesh_builder_;
            }

            absl::FixedArray< std::vector< index_t > > get_cell_vertices(
                absl::Span< const int64_t > connectivity,
                absl::Span< const int64_t > offsets ) const
            {
                absl::FixedArray< std::vector< index_t > > cell_vertices(
                    offsets.size() );
                int64_t prev_offset{ 0 };
                for( const auto p : Indices{ offsets } )
                {
                    const auto cur_offset = offsets[p];
                    auto& vertices = cell_vertices[p];
                    vertices.reserve( cur_offset - prev_offset );
                    for( const auto v : Range{ prev_offset, cur_offset } )
                    {
                        vertices.push_back(
                            static_cast< index_t >( connectivity[v] ) );
                    }
                    prev_offset = cur_offset;
                }
                return cell_vertices;
            }

        private:
            void read_vtk_object( const pugi::xml_node& vtk_object ) final
            {
                for( const auto& piece : vtk_object.children( "Piece" ) )
                {
                    read_vtk_points( piece );
                    read_vtk_cells( piece );
                }
            }

            void is_vtk_object_loadable( const pugi::xml_node& vtk_object,
                std::vector< Percentage >& percentages ) const final
            {
                for( const auto& piece : vtk_object.children( "Piece" ) )
                {
                    percentages.emplace_back( is_vtk_cells_loadable( piece ) );
                }
            }

            void read_vtk_points( const pugi::xml_node& piece )
            {
                const auto nb_points =
                    this->read_attribute( piece, "NumberOfPoints" );
                const auto vertex_offset = build_points( piece, nb_points );
                this->read_data( piece.child( "PointData" ), vertex_offset,
                    this->mesh().vertex_attribute_manager() );
            }

            virtual void read_vtk_cells( const pugi::xml_node& piece ) = 0;

            virtual Percentage is_vtk_cells_loadable(
                const pugi::xml_node& piece ) const = 0;

            index_t build_points(
                const pugi::xml_node& piece, index_t nb_points )
            {
                const auto points = read_points( piece, nb_points );
                const auto offset =
                    this->builder().create_vertices( nb_points );
                for( const auto p : Range{ nb_points } )
                {
                    this->builder().set_point( offset + p, points[p] );
                }
                return offset;
            }

            template < typename T >
            absl::FixedArray< Point3D > get_points(
                const std::vector< T >& coords )
            {
                OPENGEODE_ASSERT( coords.size() % 3 == 0,
                    "[VTKInput::get_points] Number of "
                    "coordinates is not multiple of 3" );
                const auto nb_points = coords.size() / 3;
                absl::FixedArray< Point3D > points( nb_points );
                for( const auto p : Range{ nb_points } )
                {
                    for( const auto c : Range{ 3 } )
                    {
                        points[p].set_value( c, coords[3 * p + c] );
                    }
                }
                return points;
            }

            absl::FixedArray< Point3D > read_points(
                const pugi::xml_node& piece, index_t nb_points )
            {
                const auto points =
                    piece.child( "Points" ).child( "DataArray" );
                const auto nb_components =
                    this->read_attribute( points, "NumberOfComponents" );
                const auto type = points.attribute( "type" ).value();
                OPENGEODE_EXCEPTION( this->match( type, "Float32" )
                                         || this->match( type, "Float64" ),
                    "[VTKInput::read_points] Cannot read points of type ", type,
                    ". Only Float32 and Float64 are accepted" );
                OPENGEODE_EXCEPTION( nb_components == 3,
                    "[VTKInput::read_points] Trying to import 2D VTK object "
                    "into a 3D Surface is not allowed" );
                const auto format = points.attribute( "format" ).value();
                if( this->match( format, "appended" ) )
                {
                    if( this->match( type, "Float32" ) )
                    {
                        return decode_points< float >(
                            this->read_appended_data( points ), nb_points );
                    }
                    return decode_points< double >(
                        this->read_appended_data( points ), nb_points );
                }
                else
                {
                    const auto coords_string =
                        absl::StripAsciiWhitespace( points.child_value() );
                    if( this->match( format, "ascii" ) )
                    {
                        auto string = to_string( coords_string );
                        absl::RemoveExtraAsciiWhitespace( &string );
                        const auto coords =
                            read_ascii_coordinates( string, nb_points );
                        OPENGEODE_ASSERT( coords.size() == 3 * nb_points,
                            "[VTKInput::read_points] Wrong number of "
                            "coordinates" );
                        return get_points( coords );
                    }
                    if( this->match( type, "Float32" ) )
                    {
                        return decode_points< float >(
                            coords_string, nb_points );
                    }
                    return decode_points< double >( coords_string, nb_points );
                }
            }

            template < typename T >
            absl::FixedArray< Point3D > decode_points(
                std::string_view coords_string, index_t nb_points )
            {
                const auto coords = this->template decode< T >( coords_string );
                geode_unused( nb_points );
                OPENGEODE_ASSERT( coords.size() == 3 * nb_points,
                    "[VTKInput::read_points] Wrong number of coordinates" );
                return get_points( coords );
            }

            std::vector< double > read_ascii_coordinates(
                std::string_view coords, index_t nb_points )
            {
                std::vector< double > results;
                results.reserve( 3 * nb_points );
                for( auto string : absl::StrSplit( coords, ' ' ) )
                {
                    double coord;
                    const auto ok = absl::SimpleAtod( string, &coord );
                    OPENGEODE_EXCEPTION( ok,
                        "[VTKInput::read_ascii_coordinates] Failed to read "
                        "coordinate" );
                    results.push_back( coord );
                }
                return results;
            }

        private:
            std::unique_ptr< MeshBuilder > mesh_builder_;
        }; // namespace detail
    } // namespace detail
} // namespace geode
