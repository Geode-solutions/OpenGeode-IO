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

#include <fstream>

#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
#include <absl/strings/match.h>
#include <absl/strings/numbers.h>
#include <pugixml.hpp>
#include <zlib-ng.h>

#include <geode/geometry/point.h>

#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/core/geode_polygonal_surface.h>

namespace
{
    bool match( absl::string_view query, absl::string_view ref )
    {
        return absl::StartsWith( query, ref ) && absl::EndsWith( query, ref );
    }

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
                match( root_.attribute( "type" ).value(), "PolyData" ),
                "[VTPInput::read_root_attributes] VTK File type should be "
                "PolyData" );
            if( match( root_.attribute( "byte_order" ).value(), "BigEndian" ) )
            {
                little_endian_ = false;
            }
            const auto compressor = root_.attribute( "compressor" ).value();
            OPENGEODE_EXCEPTION(
                absl::string_view( compressor ).empty()
                    || match( root_.attribute( "compressor" ).value(),
                        "vtkZLibDataCompressor" ),
                "[VTPInput::read_root_attributes] Only vtkZLibDataCompressor "
                "is supported for now" );
            if( !absl::string_view( compressor ).empty() )
            {
                compressed_ = true;
            }
        }

        void read_polydata( const pugi::xml_node& polydata )
        {
            for( const auto& piece : polydata.children( "Piece" ) )
            {
                geode::index_t nb_polygons;
                absl::SimpleAtoi(
                    piece.attribute( "NumberOfPolys" ).value(), &nb_polygons );
                if( nb_polygons == 0 )
                {
                    continue;
                }
                geode::index_t nb_points;
                absl::SimpleAtoi(
                    piece.attribute( "NumberOfPoints" ).value(), &nb_points );
                // read_point_data( piece ); TODO for attributes
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
            absl::FixedArray< geode::index_t > new_polygons(
                polygon_vertices.size() );
            std::iota( new_polygons.begin(), new_polygons.end(),
                surface_.nb_polygons() );
            for( const auto& pv : polygon_vertices )
            {
                surface_builder_->create_polygon( pv );
            }
            surface_builder_->compute_polygon_adjacencies( new_polygons );
        }

        template < typename T >
        std::vector< T > decode( absl::string_view input )
        {
            auto clean_input = absl::StripAsciiWhitespace( input );
            const geode::index_t fixed_header_length{
                16
            }; // 4 series of 4 characters is needed to encode the 3 values in
               // the fixed header (3 * 4 bytes)
            auto fixed_header = clean_input.substr( 0, fixed_header_length );
            std::string bytes;
            auto decode_status = absl::Base64Unescape( fixed_header, &bytes );
            OPENGEODE_EXCEPTION( decode_status,
                "[VTPInput::decode] Error in decoding base64 "
                "data for fixed header" );
            OPENGEODE_ASSERT( bytes.size() == 12,
                absl::StrCat( "[VTPInput::decode] Fixed header size is wrong "
                              "(should be 12 bytes, got ",
                    bytes.size(), " bytes)" ) );
            const auto fixed_header_values =
                reinterpret_cast< const geode::index_t* >(
                    bytes.c_str() ); // should be unsigned long if UInt64
            const auto nb_data_blocks = fixed_header_values[0];
            if( nb_data_blocks == 0 )
            {
                return std::vector< T >{};
            }
            const auto uncompressed_block_size = fixed_header_values[1];
            const auto nb_characters =
                std::ceil( nb_data_blocks * 32. / 24. )
                * 4; // for each blocks 4 bytes (32 bits) are needed to encode a
                     // value, encoded by packs of 3 bytes (24 bits) represented
                     // by 4 characters
            auto optional_header =
                clean_input.substr( fixed_header_length, nb_characters );
            decode_status = absl::Base64Unescape( optional_header, &bytes );
            OPENGEODE_EXCEPTION( decode_status,
                "[VTPInput::decode] Error in decoding base64 "
                "data for optional header" );
            OPENGEODE_ASSERT( bytes.size() == nb_data_blocks * 4,
                absl::StrCat(
                    "[VTPInput::decode] Fixed header size is wrong (should be ",
                    nb_data_blocks * 4, " bytes, got ", bytes.size(),
                    " bytes)" ) );
            const auto optional_header_values =
                reinterpret_cast< const geode::index_t* >(
                    bytes.c_str() ); // should be unsigned long if UInt64
            geode::index_t sum_compressed_block_size{ 0 };
            absl::FixedArray< geode::index_t > compressed_blocks_size(
                nb_data_blocks );
            for( const auto b : geode::Range{ nb_data_blocks } )
            {
                compressed_blocks_size[b] = optional_header_values[b];
                sum_compressed_block_size += optional_header_values[b];
            }

            const auto data_offset = fixed_header_length + nb_characters;
            auto data = clean_input.substr(
                data_offset, clean_input.size() - data_offset );
            decode_status = absl::Base64Unescape( data, &bytes );
            OPENGEODE_EXCEPTION( decode_status,
                "[VTPInput::decode] Error in decoding base64 data" );
            const auto compressed_data_bytes =
                reinterpret_cast< const unsigned char* >( bytes.c_str() );

            std::vector< T > result;
            result.reserve( std::ceil(
                nb_data_blocks * uncompressed_block_size / sizeof( T ) ) );
            geode::index_t cur_data_offset{ 0 };
            for( const auto b : geode::Range{ nb_data_blocks } )
            {
                const auto compressed_data_length =
                    // 3 * sum_compressed_block_size;
                    sum_compressed_block_size;
                size_t decompressed_data_length =
                    nb_data_blocks * uncompressed_block_size;
                absl::FixedArray< uint8_t > decompressed_data_bytes(
                    decompressed_data_length );
                const auto uncompress_result = zng_uncompress(
                    decompressed_data_bytes.data(), &decompressed_data_length,
                    &compressed_data_bytes[cur_data_offset],
                    compressed_data_length );
                OPENGEODE_EXCEPTION( uncompress_result == Z_OK,
                    "[VTPInput::decode] Error in zlib decompressing data" );
                const auto values = reinterpret_cast< const T* >(
                    decompressed_data_bytes.data() );
                const auto nb_values = decompressed_data_length / sizeof( T );
                for( const auto v : geode::Range{ nb_values } )
                {
                    result.push_back( values[v] );
                }
                cur_data_offset += compressed_blocks_size[b];
            }
            return result;
        }

        template < typename T >
        absl::FixedArray< geode::Point3D > get_points(
            const std::vector< T >& coords )
        {
            OPENGEODE_ASSERT( coords.size() % 3 == 0,
                "[VTPInput::get_points] Number of "
                "coordinates is not multiple of 3" );
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
            geode::index_t nb_components;
            absl::SimpleAtoi( points.attribute( "NumberOfComponents" ).value(),
                &nb_components );
            OPENGEODE_EXCEPTION( nb_components == 3,
                "[VTPInput::read_points] Trying to import 2D VTK PolyData into "
                "a 3D Surface is not allowed" );
            const auto format = points.attribute( "format" ).value();
            const auto coords_string = points.child_value();
            if( match( format, "ascii" ) )
            {
                const auto coords =
                    read_ascii_coordinates( coords_string, nb_points );
                OPENGEODE_ASSERT( coords.size() == 3 * nb_points,
                    "[VTPInput::read_points] Wrong number of coordinates" );
                return get_points( coords );
            }
            const auto coords = decode< float >( coords_string );
            OPENGEODE_ASSERT( coords.size() == 3 * nb_points,
                "[VTPInput::read_points] Wrong number of coordinates" );
            return get_points( coords );
        }

        std::vector< double > read_ascii_coordinates(
            absl::string_view coords, geode::index_t nb_points )
        {
            std::vector< double > results;
            results.reserve( 3 * nb_points );
            std::istringstream iss( coords.data() );
            std::string c;
            double coord;
            iss >> c;
            while( !iss.eof() )
            {
                absl::SimpleAtod( c, &coord );
                results.push_back( coord );
                iss >> c;
            }
            return results;
        }

        absl::FixedArray< std::vector< geode::index_t > > get_polygon_vertices(
            absl::Span< const geode::index_t > connectivity,
            absl::Span< const geode::index_t > offsets )
        {
            absl::FixedArray< std::vector< geode::index_t > > polygon_vertices(
                offsets.size() );
            geode::index_t prev_offset{ 0 };
            for( const auto p : geode::Range{ offsets.size() } )
            {
                const auto cur_offset = offsets[p];
                auto& vertices = polygon_vertices[p];
                vertices.reserve( cur_offset - prev_offset );
                for( const auto v : geode::Range{ prev_offset, cur_offset } )
                {
                    vertices.push_back( connectivity[v] );
                }
                prev_offset = cur_offset;
            }
            return polygon_vertices;
        }

        absl::FixedArray< std::vector< geode::index_t > > read_polygons(
            const pugi::xml_node& piece, geode::index_t nb_polygons )
        {
            std::vector< int64_t > offsets_values;
            std::vector< int64_t > connectivity_values;
            for( const auto& data :
                piece.child( "Polys" ).children( "DataArray" ) )
            {
                if( match( data.attribute( "Name" ).value(), "offsets" ) )
                {
                    offsets_values = read_data_array< int64_t >( data );
                    OPENGEODE_ASSERT( offsets_values.size() == nb_polygons,
                        "[VTPInput::read_points] Wrong number of offsets" );
                }
                else if( match( data.attribute( "Name" ).value(),
                             "connectivity" ) )
                {
                    connectivity_values = read_data_array< int64_t >( data );
                }
            }
            return get_polygon_vertices(
                cast_data_array< int64_t, geode::index_t >(
                    connectivity_values ),
                cast_data_array< int64_t, geode::index_t >( offsets_values ) );
        }

        template < typename T_in, typename T_out >
        absl::FixedArray< T_out > cast_data_array(
            const std::vector< T_in >& data )
        {
            absl::FixedArray< T_out > casted( data.size() );
            for( const auto& d : geode::Range{ data.size() } )
            {
                casted[d] = static_cast< T_out >( data[d] );
            }
            return casted;
        }

        template < typename T >
        std::vector< T > read_data_array( const pugi::xml_node& data )
        {
            const auto format = data.attribute( "format" ).value();
            const auto data_string = data.child_value();
            if( match( format, "ascii" ) )
            {
                return read_ascii_data_array< T >( data_string );
            }
            return decode< T >( data_string );
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
            T value;
            while( !iss.eof() )
            {
                absl::SimpleAtoi( c, &value ); // TODO map between SimpleAtoX
                                               // and T for full compatibility
                results.push_back( value );
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
