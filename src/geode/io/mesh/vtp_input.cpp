/*
 * Copyright (c) 2019 - 2020 Geode-solutions
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

#include <geode/io/mesh/private/vtp_input.h>

#include <absl/strings/escaping.h>
#include <absl/strings/numbers.h>
#include <bitset>
#include <fstream>
#include <iostream>
#include <locale>
#include <pugixml.hpp>
#include <zlib-ng.h>

#include <geode/geometry/point.h>

#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/core/geode_polygonal_surface.h>

namespace
{
    class VTPInputImpl
    {
    public:
        VTPInputImpl( absl::string_view filename,
            geode::PolygonalSurface3D& polygonal_surface )
            : file_( filename.data() ),
              surface_( polygonal_surface ),
              surface_builder_{ geode::PolygonalSurfaceBuilder3D::create(
                  polygonal_surface ) }
        {
            OPENGEODE_EXCEPTION( file_.good(),
                "[VTPInput] Error while opening file: ", filename );
            const auto ok = document_.load_file( filename.data() );
            OPENGEODE_EXCEPTION(
                ok, "[VTPInput] Error while parsing file: ", filename );
            root_ = document_.child( "VTKFile" );
        }

        void read_file()
        {
            read_root_attributes();
            for( const auto& polydata : root_.children( "PolyData" ) )
            {
                read_polydata( polydata );
            }
        }

    private:
        void read_root_attributes()
        {
            OPENGEODE_EXCEPTION(
                strcmp( root_.attribute( "type" ).value(), "PolyData" ) == 0,
                "[VTPInput] VTK File type should be PolyData" );
            if( strcmp( root_.attribute( "byte_order" ).value(), "BigEndian" )
                == 0 )
            {
                little_endian_ = false;
            }
            const auto compressor = root_.attribute( "compressor" ).value();
            OPENGEODE_EXCEPTION(
                absl::string_view( compressor ).empty()
                    || strcmp( root_.attribute( "compressor" ).value(),
                           "vtkZLibDataCompressor" )
                           == 0,
                "[VTPInput] Only vtkZLibDataCompressor is supported for now" );
            if( !absl::string_view( compressor ).empty() )
            {
                compressed_ = true;
            }
        }

        void read_polydata( const pugi::xml_node& polydata )
        {
            for( const auto& piece : polydata.children( "Piece" ) )
            {
                const auto nb_polygons =
                    std::stoul( piece.attribute( "NumberOfPolys" ).value() );
                if( nb_polygons == 0 )
                {
                    continue;
                }
                const auto nb_points =
                    std::stoul( piece.attribute( "NumberOfPoints" ).value() );
                // read_point_data( piece );
                // read_cell_data( piece );
                const auto points = read_points( piece, nb_points );
                const auto polygons = read_polygons( piece, nb_polygons );

                build_piece( points, polygons );
            }
        }

        void build_piece( const absl::FixedArray< geode::Point3D >& points,
            const absl::FixedArray< std::vector< geode::index_t > >&
                polygon_vertices )
        {
            const auto nb_points = points.size();
            const auto offset = surface_builder_->create_vertices( nb_points );
            for( const auto p : geode::Range{ nb_points } )
            {
                surface_builder_->set_point( offset + p, points[p] );
            }
            for( const auto& pv : polygon_vertices )
            {
                surface_builder_->create_polygon( pv );
            }
        }

        absl::string_view remove_spaces( absl::string_view in )
        {
            auto left = in.begin();
            while( isspace( *left ) )
            {
                left++;
            }
            for( ;; ++left )
            {
                if( left == in.end() )
                    return absl::string_view();
                if( !isspace( *left ) )
                    break;
            }
            auto right = in.end() - 1;
            while( isspace( *right ) && right > left )
            {
                right--;
            }
            return absl::string_view( left, std::distance( left, right ) + 1 );
        }

        template < typename T >
        std::vector< T > decode( absl::string_view input )
        {
            auto clean_input = remove_spaces( input );
            auto fixed_header = clean_input.substr( 0, 16 );
            std::string bytes;
            auto decode_status = absl::Base64Unescape( fixed_header, &bytes );
            OPENGEODE_EXCEPTION(
                decode_status, "Pb decode base64 (fixed header)" );
            OPENGEODE_ASSERT( bytes.size() == 12, "fixed header size wrong" );
            const auto fixed_header_values =
                reinterpret_cast< const geode::index_t* >(
                    bytes.c_str() ); // should be unsigned long if UInt64
            const auto nb_data_blocks = fixed_header_values[0];
            if( nb_data_blocks == 0 )
            {
                return std::vector< T >{};
            }
            OPENGEODE_EXCEPTION( nb_data_blocks == 1,
                "More than one data block to decode is not supported yet" );
            const auto uncompressed_block_size = fixed_header_values[1];
            // const auto last_partial_block_size = fixed_header_values[2];
            auto optional_header = clean_input.substr( 16, 8 );
            decode_status = absl::Base64Unescape( optional_header, &bytes );
            OPENGEODE_EXCEPTION(
                decode_status, "Pb decode base64 (optional header)" );
            OPENGEODE_ASSERT( bytes.size() == 4, "optional header size wrong" );
            const auto optional_header_values =
                reinterpret_cast< const geode::index_t* >(
                    bytes.c_str() ); // should be unsigned long if UInt64
            const auto compressed_block_size = optional_header_values[0];

            const auto data_offset = 16 + 8 * nb_data_blocks;
            auto data = clean_input.substr(
                data_offset, clean_input.size() - data_offset );
            decode_status = absl::Base64Unescape( data, &bytes );
            OPENGEODE_EXCEPTION( decode_status, "Pb decode base64 (data)" );

            const auto compressed_data_length =
                3 * compressed_block_size; // 3 * header_values[3]
            size_t decompressed_data_length =
                uncompressed_block_size; // header_values[1]
            uint8_t decompressed_data_bytes[decompressed_data_length];
            const auto compressed_data_bytes =
                reinterpret_cast< const unsigned char* >( bytes.c_str() );
            const auto uncompress_result = zng_uncompress(
                decompressed_data_bytes, &decompressed_data_length,
                compressed_data_bytes, compressed_data_length );
            OPENGEODE_EXCEPTION( uncompress_result == Z_OK, "Pb zlib" );
            const auto values =
                reinterpret_cast< const T* >( decompressed_data_bytes );
            const auto nb_values = decompressed_data_length / sizeof( T );
            std::vector< T > result( nb_values );
            for( const auto v : geode::Range{ nb_values } )
            {
                result[v] = values[v];
            }
            return result;
        }

        template < typename T >
        absl::FixedArray< geode::Point3D > get_points(
            const std::vector< T >& coords )
        {
            OPENGEODE_ASSERT(
                coords.size() % 3 == 0, "Pb nb coords not match 3D" );
            const auto nb_points = coords.size() / 3;
            absl::FixedArray< geode::Point3D > points( nb_points );
            for( const auto p : geode::Range{ nb_points } )
            {
                for( const auto c : geode::Range{ 3 } )
                {
                    points[p].set_value( c, coords[3 * p + c] );
                }
            }
            return points;
        }

        absl::FixedArray< geode::Point3D > read_points(
            const pugi::xml_node& piece, geode::index_t nb_points )
        {
            const auto points = piece.child( "Points" ).child( "DataArray" );
            OPENGEODE_EXCEPTION(
                std::stoul( points.attribute( "NumberOfComponents" ).value() )
                    == 3,
                "[VTPInput] Trying to import 2D VTK PolyData into a 3D Surface "
                "is not allowed" );
            const auto format = points.attribute( "format" ).value();
            const auto coords_string = points.child_value();
            if( strcmp( format, "ascii" ) == 0 )
            {
                const auto coords =
                    read_ascii_coordinates( coords_string, nb_points );
                OPENGEODE_ASSERT( coords.size() == 3 * nb_points,
                    "Wrong number of coordinates" );
                return get_points( coords );
            }
            const auto coords = decode< float >( coords_string );
            OPENGEODE_ASSERT(
                coords.size() == 3 * nb_points, "Wrong number of coordinates" );
            return get_points( coords );
        }

        std::vector< double > read_ascii_coordinates(
            absl::string_view coords, geode::index_t nb_points )
        {
            std::vector< double > results;
            results.reserve( 3 * nb_points );
            std::istringstream iss( coords.data() );
            std::string c;
            iss >> c;
            while( !iss.eof() )
            {
                results.push_back( std::stod( c ) );
                iss >> c;
            }
            return results;
        }

        absl::FixedArray< std::vector< geode::index_t > > get_polygon_vertices(
            const std::vector< geode::index_t >& connectivity,
            const std::vector< geode::index_t >& offsets )
        {
            for( const auto p : offsets )
            {
                DEBUG( p );
            }
            absl::FixedArray< std::vector< geode::index_t > > polygon_vertices(
                offsets.size() );
            geode::index_t prev_offset{ 0 };
            for( const auto p : geode::Range{ offsets.size() } )
            {
                DEBUG( "=====" );
                const auto cur_offset = offsets[p];
                auto& vertices = polygon_vertices[p];
                DEBUG( cur_offset - prev_offset - 1 );
                vertices.reserve( cur_offset - prev_offset - 1 );
                for( const auto v : geode::Range{ prev_offset, cur_offset } )
                {
                    vertices.push_back( connectivity[v] );
                    DEBUG( vertices.back() );
                }
                prev_offset = cur_offset;
            }
            return polygon_vertices;
        }

        absl::FixedArray< std::vector< geode::index_t > > read_polygons(
            const pugi::xml_node& piece, geode::index_t nb_polygons )
        {
            std::vector< geode::index_t > offsets;
            std::vector< geode::index_t > connectivity;
            for( const auto& data :
                piece.child( "Polys" ).children( "DataArray" ) )
            {
                if( strcmp( data.attribute( "Name" ).value(), "offsets" ) == 0 )
                {
                    offsets = read_data_array( data );
                }
                else if( strcmp(
                             data.attribute( "Name" ).value(), "connectivity" )
                         == 0 )
                {
                    connectivity = read_data_array( data );
                }
            }
            return get_polygon_vertices( connectivity, offsets );
        }

        std::vector< geode::index_t > read_data_array(
            const pugi::xml_node& data )
        {
            const auto format = data.attribute( "format" ).value();
            const auto data_string = data.child_value();
            if( strcmp( format, "ascii" ) == 0 )
            {
                return read_ascii_data_array< geode::index_t >( data_string );
            }
            return decode< geode::index_t >( data_string );
        }

        template < typename T >
        std::vector< T > read_ascii_data_array(
            absl::string_view data, geode::index_t nb_values = geode::NO_ID )
        {
            std::vector< T > results;
            if( nb_values != geode::NO_ID )
            {
                results.reserve( nb_values );
            }
            std::istringstream iss( data.data() );
            std::string c;
            iss >> c;
            while( !iss.eof() )
            {
                results.push_back(
                    std::stoul( c ) ); // map between stoXX and T ?
                iss >> c;
            }
            return results;
        }

    private:
        std::ifstream file_;
        geode::PolygonalSurface3D& surface_;
        std::unique_ptr< geode::PolygonalSurfaceBuilder3D > surface_builder_;
        pugi::xml_document document_;
        pugi::xml_node root_;
        bool little_endian_{ true };
        bool compressed_{ false };
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void VTPInput::do_read()
        {
            VTPInputImpl impl{ filename(), polygonal_surface() };
            impl.read_file();
        }
    } // namespace detail
} // namespace geode
