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

#include <geode/io/mesh/common.h>

#include <fstream>

#include <pugixml.hpp>

#include <zlib.h>

#include <absl/strings/escaping.h>
#include <absl/strings/str_split.h>

#include <geode/basic/attribute_manager.h>

#include <geode/geometry/point.h>

namespace geode
{
    namespace detail
    {
        template < typename Mesh, typename MeshBuilder >
        class VTKInputImpl
        {
        public:
            virtual ~VTKInputImpl() = default;

            void read_file()
            {
                read_root_attributes();
                read_appended_data();
                for( const auto& vtk_object : root_.children( type_ ) )
                {
                    read_vtk_object( vtk_object );
                }
            }

        protected:
            VTKInputImpl( absl::string_view filename,
                Mesh& polygonal_surface,
                const char* type )
                : file_{ to_string( filename ) },
                  mesh_( polygonal_surface ),
                  mesh_builder_{ MeshBuilder::create( polygonal_surface ) },
                  type_{ type }
            {
                OPENGEODE_EXCEPTION( file_.good(),
                    "[VTKInput] Error while opening file: ", filename );
                const auto status =
                    document_.load_file( to_string( filename ).c_str() );
                OPENGEODE_EXCEPTION( status, "[VTKInput] Error ",
                    status.description(), " while parsing file: ", filename );
                root_ = document_.child( "VTKFile" );
            }

            const Mesh& mesh() const
            {
                return mesh_;
            }

            MeshBuilder& builder()
            {
                return *mesh_builder_;
            }

            bool match( absl::string_view query, absl::string_view ref ) const
            {
                return absl::StartsWith( query, ref )
                       && absl::EndsWith( query, ref );
            }

            index_t read_attribute(
                const pugi::xml_node& piece, absl::string_view attribute ) const
            {
                index_t value;
                const auto ok = absl::SimpleAtoi(
                    piece.attribute( attribute.data() ).value(), &value );
                OPENGEODE_EXCEPTION( ok,
                    "[VTKInput::read_attribute] Failed to read attribute: ",
                    attribute );
                return value;
            }

            template < typename T >
            std::vector< T > read_integer_data_array(
                const pugi::xml_node& data )
            {
                const auto format = data.attribute( "format" ).value();
                if( match( format, "appended" ) )
                {
                    return decode< T >( read_appended_data( data ) );
                }
                else
                {
                    const auto data_string =
                        absl::StripAsciiWhitespace( data.child_value() );
                    if( match( format, "ascii" ) )
                    {
                        auto string = to_string( data_string );
                        absl::RemoveExtraAsciiWhitespace( &string );
                        return read_ascii_integer_data_array< T >( string );
                    }
                    return decode< T >( data_string );
                }
            }

            template < typename T >
            std::vector< T > read_uint8_data_array( const pugi::xml_node& data )
            {
                const auto format = data.attribute( "format" ).value();
                if( match( format, "appended" ) )
                {
                    return decode< T >( read_appended_data( data ) );
                }
                else
                {
                    const auto data_string =
                        absl::StripAsciiWhitespace( data.child_value() );
                    if( match( format, "ascii" ) )
                    {
                        auto string = to_string( data_string );
                        absl::RemoveExtraAsciiWhitespace( &string );
                        return read_ascii_uint8_data_array< T >( string );
                    }
                    return decode< T >( data_string );
                }
            }

            template < typename T >
            std::vector< T > read_float_data_array( const pugi::xml_node& data )
            {
                const auto format = data.attribute( "format" ).value();
                if( match( format, "appended" ) )
                {
                    return decode< T >( read_appended_data( data ) );
                }
                else
                {
                    const auto data_string =
                        absl::StripAsciiWhitespace( data.child_value() );
                    if( match( format, "ascii" ) )
                    {
                        auto string = to_string( data_string );
                        absl::RemoveExtraAsciiWhitespace( &string );
                        return read_ascii_float_data_array< T >( string );
                    }
                    return decode< T >( data_string );
                }
            }

            absl::FixedArray< std::vector< index_t > > get_cell_vertices(
                absl::Span< const int64_t > connectivity,
                absl::Span< const int64_t > offsets )
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

            template < typename Out, typename In >
            std::vector< Out > cast_to( absl::Span< const In > values )
            {
                std::vector< Out > result( values.size() );
                for( const auto v : Indices{ values } )
                {
                    result[v] = static_cast< Out >( values[v] );
                }
                return result;
            }

            template < typename T >
            void build_attribute( AttributeManager& manager,
                absl::string_view name,
                absl::Span< const T > values,
                index_t nb_components,
                index_t offset )
            {
                OPENGEODE_EXCEPTION( values.size() % nb_components == 0,
                    "[VTKInput::build_attribute] Number of attribute "
                    "values is not a multiple of number of components" );
                if( manager.find_generic_attribute( name ) )
                {
                    return;
                }
                if( nb_components == 1 )
                {
                    auto attribute =
                        manager
                            .find_or_create_attribute< VariableAttribute, T >(
                                name, T{} );
                    for( const auto i : Indices{ values } )
                    {
                        attribute->set_value( i + offset, values[i] );
                    }
                }
                else if( nb_components == 2 )
                {
                    create_attribute< std::array< T, 2 >, T >(
                        manager, {}, values, nb_components, name, offset );
                }
                else if( nb_components == 3 )
                {
                    create_attribute< std::array< T, 3 >, T >(
                        manager, {}, values, nb_components, name, offset );
                }
                else
                {
                    create_attribute< std::vector< T >, T >( manager,
                        std::vector< T >( nb_components ), values,
                        nb_components, name, offset );
                }
            }

            void read_attribute_data( const pugi::xml_node& data,
                index_t offset,
                AttributeManager& attribute_manager )
            {
                const auto data_array_name = data.attribute( "Name" ).value();
                const auto data_array_type = data.attribute( "type" ).value();
                index_t nb_components{ 1 };
                if( const auto data_nb_components =
                        data.attribute( "NumberOfComponents" ) )
                {
                    nb_components =
                        read_attribute( data, "NumberOfComponents" );
                }
                if( match( data_array_type, "Float64" )
                    || match( data_array_type, "Float32" ) )
                {
                    const auto attribute_values =
                        read_float_data_array< double >( data );
                    build_attribute< double >( attribute_manager,
                        data_array_name, attribute_values, nb_components,
                        offset );
                }
                else if( match( data_array_type, "Int64" )
                         || match( data_array_type, "Int32" )
                         || match( data_array_type, "UInt64" ) )
                {
                    const auto min_value = read_attribute( data, "RangeMin" );
                    const auto max_value = read_attribute( data, "RangeMax" );
                    if( min_value >= 0
                        && max_value < std::numeric_limits< index_t >::max() )
                    {
                        const auto attribute_values =
                            read_integer_data_array< index_t >( data );
                        build_attribute< index_t >( attribute_manager,
                            data_array_name, attribute_values, nb_components,
                            offset );
                    }
                    else
                    {
                        const auto attribute_values =
                            read_integer_data_array< long int >( data );
                        build_attribute< long int >( attribute_manager,
                            data_array_name, attribute_values, nb_components,
                            offset );
                    }
                }
                else if( match( data_array_type, "Int8" ) )
                {
                    // const auto attribute_values =
                    //     read_uint8_data_array< int8_t >( data );
                    // const auto attribute_values_as_int =
                    //     cast_to< int >( attribute_values );
                    // build_attribute( attribute_manager, data_array_name,
                    //     attribute_values_as_int, nb_components, offset );
                }
                else if( match( data_array_type, "UInt8" ) )
                {
                    const auto attribute_values =
                        read_uint8_data_array< uint8_t >( data );
                    const auto attribute_values_as_int =
                        cast_to< index_t, uint8_t >( attribute_values );
                    build_attribute< index_t >( attribute_manager,
                        data_array_name, attribute_values_as_int, nb_components,
                        offset );
                }
                else
                {
                    throw OpenGeodeException(
                        "[VTKInput::read_point_data] Attribute of type ",
                        data_array_type, " is not supported" );
                }
            }

        private:
            template < typename Container, typename T >
            void create_attribute( AttributeManager& manager,
                const Container& default_value,
                absl::Span< const T > values,
                index_t nb_components,
                absl::string_view name,
                index_t offset )
            {
                auto attribute =
                    manager.find_or_create_attribute< VariableAttribute,
                        Container >( name, default_value );
                for( const auto i : Range{ values.size() / nb_components } )
                {
                    for( const auto c : Range{ nb_components } )
                    {
                        const auto& new_value = values[nb_components * i + c];
                        attribute->modify_value(
                            i + offset, [&new_value, &c]( Container& value ) {
                                value[c] = new_value;
                            } );
                    }
                }
            }

            void read_root_attributes()
            {
                OPENGEODE_EXCEPTION(
                    match( root_.attribute( "type" ).value(), type_ ),
                    "[VTKInput::read_root_attributes] VTK File type should be ",
                    type_ );
                little_endian_ = match(
                    root_.attribute( "byte_order" ).value(), "LittleEndian" );
                OPENGEODE_EXCEPTION( little_endian_,
                    "[VTKInput::read_root_attributes] Big "
                    "Endian not supported" );
                const auto compressor = root_.attribute( "compressor" ).value();
                OPENGEODE_EXCEPTION(
                    absl::string_view( compressor ).empty()
                        || match( compressor, "vtkZLibDataCompressor" ),
                    "[VTKInput::read_root_attributes] Only "
                    "vtkZLibDataCompressor is supported for now" );
                compressed_ = !absl::string_view( compressor ).empty();

                if( const auto header_type = root_.attribute( "header_type" ) )
                {
                    const auto& header_type_value = header_type.value();
                    OPENGEODE_EXCEPTION(
                        match( header_type_value, "UInt32" )
                            || match( header_type_value, "UInt64" ),
                        "[VTKInput::read_root_attributes] Cannot read VTKFile "
                        "with header_type ",
                        header_type_value,
                        ". Only UInt32 and Uint64 are accepted" );
                    is_uint64_ = match( header_type_value, "UInt64" );
                }
            }

            void read_appended_data()
            {
                {
                    const auto node = root_.child( "AppendedData" );
                    if( !node )
                    {
                        return;
                    }
                    OPENGEODE_EXCEPTION(
                        match( node.attribute( "encoding" ).value(), "base64" ),
                        "[VTKInput::read_appended_data] VTK AppendedData "
                        "section should be encoded" );
                    appended_data_ = node.child_value();
                }
                appended_data_ = absl::StripAsciiWhitespace( appended_data_ );
                appended_data_.remove_prefix( 1 ); // skip first char: '_'
            }

            void read_vtk_object( const pugi::xml_node& vtk_object )
            {
                for( const auto& piece : vtk_object.children( "Piece" ) )
                {
                    read_vtk_points( piece );
                    read_vtk_cells( piece );
                }
            }

            void read_vtk_points( const pugi::xml_node& piece )
            {
                const auto nb_points =
                    read_attribute( piece, "NumberOfPoints" );
                const auto vertex_offset = build_points( piece, nb_points );
                read_point_data( piece.child( "PointData" ), vertex_offset );
            }

            virtual void read_vtk_cells( const pugi::xml_node& piece ) = 0;

            index_t build_points(
                const pugi::xml_node& piece, index_t nb_points )
            {
                const auto points = read_points( piece, nb_points );
                const auto offset = mesh_builder_->create_vertices( nb_points );
                for( const auto p : Range{ nb_points } )
                {
                    mesh_builder_->set_point( offset + p, points[p] );
                }
                return offset;
            }

            void read_point_data(
                const pugi::xml_node& point_data, index_t offset )
            {
                for( const auto& data : point_data.children( "DataArray" ) )
                {
                    read_attribute_data(
                        data, offset, mesh_.vertex_attribute_manager() );
                }
            }

            template < typename T >
            std::vector< T > decode( absl::string_view input )
            {
                if( !compressed_ )
                {
                    if( is_uint64_ )
                    {
                        return templated_decode_uncompressed< T, uint64_t >(
                            input );
                    }
                    return templated_decode_uncompressed< T, uint32_t >(
                        input );
                }
                if( is_uint64_ )
                {
                    return templated_decode< T, uint64_t >( input );
                }
                return templated_decode< T, uint32_t >( input );
            }

            template < typename T, typename UInt >
            std::vector< T > templated_decode_uncompressed(
                absl::string_view input )
            {
                const auto nb_chars = nb_char_needed< UInt >( 1 );
                const auto nb_bytes_input = input.substr( 0, nb_chars );
                const auto decoded_nb_bytes_input =
                    decode_base64( nb_bytes_input );
                const auto nb_data = *reinterpret_cast< const UInt* >(
                                         decoded_nb_bytes_input.c_str() )
                                     / sizeof( T );
                const auto nb_chars2 = nb_char_needed< T >( nb_data );

                const auto data = input.substr( 0, nb_chars + nb_chars2 );
                const auto decoded_data = decode_base64( data );
                const auto values = reinterpret_cast< const T* >(
                    decoded_data.c_str() + sizeof( UInt ) );
                // skip first UInt that gives the number of bytes
                const auto nb_values =
                    ( decoded_data.size() - sizeof( UInt ) ) / sizeof( T );
                std::vector< T > result( nb_values );
                for( const auto v : Range{ nb_values } )
                {
                    result[v] = values[v];
                }
                return result;
            }

            template < typename UInt, typename UInt2 >
            constexpr index_t nb_char_needed( UInt2 nb_values ) const
            {
                // to encode the nb values in base64
                // ((nb * 8 * nb bytes) bits / 6) ->ceil
                return static_cast< index_t >(
                    4
                    * std::ceil(
                        nb_values * 8. * sizeof( UInt ) / ( 6. * 4. ) ) );
            }

            template < typename T, typename UInt >
            std::vector< T > templated_decode( absl::string_view input )
            {
                const auto fixed_header_length = nb_char_needed< UInt >( 3 );
                auto fixed_header = input.substr( 0, fixed_header_length );
                const auto decoded_fixed_header = decode_base64( fixed_header );
                const auto fixed_header_values =
                    reinterpret_cast< const UInt* >(
                        decoded_fixed_header.c_str() );
                const auto nb_data_blocks = fixed_header_values[0];
                if( nb_data_blocks == 0 )
                {
                    return std::vector< T >{};
                }
                const auto uncompressed_block_size = fixed_header_values[1];
                const auto nb_characters =
                    nb_char_needed< UInt >( nb_data_blocks );
                auto optional_header =
                    input.substr( fixed_header_length, nb_characters );
                const auto decoded_optional_header =
                    decode_base64( optional_header );
                OPENGEODE_ASSERT( decoded_optional_header.size()
                                      == nb_data_blocks * sizeof( UInt ),
                    absl::StrCat( "[VTKInput::decode] Optional header size is "
                                  "wrong (should be ",
                        nb_data_blocks * sizeof( UInt ), " bytes, got ",
                        decoded_optional_header.size(), " bytes)" ) );
                const auto optional_header_values =
                    reinterpret_cast< const UInt* >(
                        decoded_optional_header.c_str() );
                UInt sum_compressed_block_size{ 0 };
                absl::FixedArray< UInt > compressed_blocks_size(
                    nb_data_blocks );
                for( const auto b : Range{ nb_data_blocks } )
                {
                    compressed_blocks_size[b] = optional_header_values[b];
                    sum_compressed_block_size += optional_header_values[b];
                }

                const auto data_offset =
                    nb_char_needed< UInt >( 3 + nb_data_blocks );
                const auto nb_data_char = static_cast< size_t >(
                    std::ceil( sum_compressed_block_size * 4. / 3. ) );
                auto data = input.substr( data_offset, nb_data_char );
                const auto decoded_data = decode_base64( data );
                const auto compressed_data_bytes =
                    reinterpret_cast< const unsigned char* >(
                        decoded_data.c_str() );

                std::vector< T > result;
                result.reserve( static_cast< size_t >(
                    std::ceil( nb_data_blocks * uncompressed_block_size
                               / sizeof( T ) ) ) );
                UInt cur_data_offset{ 0 };
                for( const auto b : Range{ nb_data_blocks } )
                {
                    const auto compressed_data_length =
                        sum_compressed_block_size;
                    unsigned long decompressed_data_length =
                        uncompressed_block_size;
                    absl::FixedArray< uint8_t > decompressed_data_bytes(
                        decompressed_data_length );
                    const auto uncompress_result =
                        uncompress( decompressed_data_bytes.data(),
                            &decompressed_data_length,
                            &compressed_data_bytes[cur_data_offset],
                            compressed_data_length );
                    OPENGEODE_EXCEPTION( uncompress_result == Z_OK,
                        "[VTKInput::decode] Error in zlib decompressing data" );
                    const auto values = reinterpret_cast< const T* >(
                        decompressed_data_bytes.data() );
                    const auto nb_values =
                        decompressed_data_length / sizeof( T );
                    for( const auto v : Range{ nb_values } )
                    {
                        result.push_back( values[v] );
                    }
                    cur_data_offset += compressed_blocks_size[b];
                }
                return result;
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

            absl::string_view read_appended_data( const pugi::xml_node& data )
            {
                const auto offset = data.attribute( "offset" ).as_uint();
                return appended_data_.substr( offset );
            }

            std::string decode_base64( absl::string_view input ) const
            {
                std::string bytes;
                auto decode_status = absl::Base64Unescape( input, &bytes );
                OPENGEODE_EXCEPTION( decode_status,
                    "[VTKInput::decode_base64] Error in decoding base64 data" );
                return bytes;
            }

            absl::FixedArray< Point3D > read_points(
                const pugi::xml_node& piece, index_t nb_points )
            {
                const auto points =
                    piece.child( "Points" ).child( "DataArray" );
                const auto nb_components =
                    read_attribute( points, "NumberOfComponents" );
                const auto type = points.attribute( "type" ).value();
                OPENGEODE_EXCEPTION(
                    match( type, "Float32" ) || match( type, "Float64" ),
                    "[VTKInput::read_points] Cannot read points of type ", type,
                    ". Only Float32 and Float64 are accepted" );
                OPENGEODE_EXCEPTION( nb_components == 3,
                    "[VTKInput::read_points] Trying to import 2D VTK object "
                    "into a 3D Surface is not allowed" );
                const auto format = points.attribute( "format" ).value();
                if( match( format, "appended" ) )
                {
                    if( match( type, "Float32" ) )
                    {
                        return decode_points< float >(
                            read_appended_data( points ), nb_points );
                    }
                    return decode_points< double >(
                        read_appended_data( points ), nb_points );
                }
                else
                {
                    const auto coords_string =
                        absl::StripAsciiWhitespace( points.child_value() );
                    if( match( format, "ascii" ) )
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
                    if( match( type, "Float32" ) )
                    {
                        return decode_points< float >(
                            coords_string, nb_points );
                    }
                    return decode_points< double >( coords_string, nb_points );
                }
            }

            template < typename T >
            absl::FixedArray< Point3D > decode_points(
                absl::string_view coords_string, index_t nb_points )
            {
                const auto coords = decode< T >( coords_string );
                geode_unused( nb_points );
                OPENGEODE_ASSERT( coords.size() == 3 * nb_points,
                    "[VTKInput::read_points] Wrong number of coordinates" );
                return get_points( coords );
            }

            std::vector< double > read_ascii_coordinates(
                absl::string_view coords, index_t nb_points )
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

            template < typename T >
            std::vector< T > read_ascii_data_array( absl::string_view data,
                bool ( *string_convert )( absl::string_view, T* ) )
            {
                std::vector< T > results;
                for( auto string : absl::StrSplit( data, ' ' ) )
                {
                    T value;
                    const auto ok = ( *string_convert )( string, &value );
                    OPENGEODE_EXCEPTION( ok, "[VTKINPUT::read_ascii_data_array]"
                                             " Failed to read value" );
                    results.push_back( value );
                }
                return results;
            }

            template < typename T >
            std::vector< T > read_ascii_integer_data_array(
                absl::string_view data )
            {
                return read_ascii_data_array< T >( data, absl::SimpleAtoi );
            }

            template < typename T >
            std::vector< T > read_ascii_uint8_data_array(
                absl::string_view data )
            {
                std::vector< T > results;
                for( auto string : absl::StrSplit( data, ' ' ) )
                {
                    results.push_back(
                        std::atoi( to_string( string ).c_str() ) );
                }
                return results;
            }

            template < typename T >
            std::vector< T > read_ascii_float_data_array(
                absl::string_view data )
            {
                return read_ascii_data_array< T >( data, absl::SimpleAtod );
            }

        private:
            std::ifstream file_;
            Mesh& mesh_;
            std::unique_ptr< MeshBuilder > mesh_builder_;
            pugi::xml_document document_;
            pugi::xml_node root_;
            const char* type_;
            bool little_endian_{ true };
            bool compressed_{ false };
            bool is_uint64_{ false };
            absl::string_view appended_data_;
        }; // namespace detail
    } // namespace detail
} // namespace geode
