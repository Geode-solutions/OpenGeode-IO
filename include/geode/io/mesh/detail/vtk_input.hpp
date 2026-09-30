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

#include <geode/io/mesh/common.hpp>

#include <cstring>
#include <fstream>
#include <limits>
#include <type_traits>

#include <pugixml.hpp>

#include <zlib.h>

#include <absl/algorithm/container.h>
#include <absl/strings/ascii.h>
#include <absl/strings/escaping.h>
#include <absl/strings/match.h>
#include <absl/strings/str_cat.h>

#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/string.hpp>
#include <geode/basic/variable_attribute.hpp>

#include <geode/geometry/point.hpp>

namespace geode
{
    namespace detail
    {
        template < typename Mesh >
        class VTKInputImpl
        {
        public:
            virtual ~VTKInputImpl() = default;

            std::unique_ptr< Mesh > read_file()
            {
                read_common_data();
                for( const auto& vtk_object : root_.children( type_ ) )
                {
                    read_vtk_object( vtk_object );
                }
                return std::move( mesh_ );
            }

            Percentage is_loadable()
            {
                read_common_data();
                std::vector< Percentage > percentages;
                for( const auto& vtk_object : root_.children( type_ ) )
                {
                    is_vtk_object_loadable( vtk_object, percentages );
                }
                if( percentages.empty() )
                {
                    return Percentage{ 0 };
                }
                const auto nb_percentages = percentages.size();
                double value{ 0 };
                for( const auto& percentage : percentages )
                {
                    value += percentage.value();
                }
                return Percentage{ value / nb_percentages };
            }

        protected:
            VTKInputImpl( std::string_view filename, const char* type )
                : file_{ to_string( filename ), std::ios::binary },
                  type_{ type }
            {
                OpenGeodeIOMeshException::check_exception( file_.good(),
                    nullptr, OpenGeodeException::TYPE::data,
                    "[VTKInput] Error while opening file: ", filename );
                file_.seekg( 0, std::ios::end );
                file_content_.resize( static_cast< size_t >( file_.tellg() ) );
                file_.seekg( 0, std::ios::beg );
                file_.read( file_content_.data(),
                    static_cast< std::streamsize >( file_content_.size() ) );
                const auto status = parse_xml();
                OpenGeodeIOMeshException::check_exception( status, nullptr,
                    OpenGeodeException::TYPE::internal, status.description(),
                    "[VTKInput] Error while parsing file: ", filename );
                root_ = document_.child( "VTKFile" );
            }

            virtual void is_vtk_object_loadable(
                const pugi::xml_node& vtk_object,
                std::vector< Percentage >& percentages ) const = 0;

            void read_common_data()
            {
                read_root_attributes();
                read_appended_data();
            }

            void initialize_mesh( std::unique_ptr< Mesh >&& mesh )
            {
                mesh_ = std::move( mesh );
            }

            Mesh& mesh()
            {
                return *mesh_;
            }

            bool match( std::string_view query, std::string_view ref ) const
            {
                return absl::StartsWith( query, ref )
                       && absl::EndsWith( query, ref );
            }

            index_t read_attribute(
                const pugi::xml_node& piece, std::string_view attribute ) const
            {
                return string_to_index(
                    piece.attribute( attribute.data() ).value() );
            }

            template < typename T >
            std::vector< T > read_data_array( const pugi::xml_node& data ) const
            {
                const std::string_view type = data.attribute( "type" ).value();
                const auto format = data.attribute( "format" ).value();
                if( match( format, "appended" ) )
                {
                    const auto block = read_appended_data( data );
                    if( is_raw_ )
                    {
                        return decode_raw_block< T >( block, type );
                    }
                    return decode_base64_block< T >( block, type );
                }
                const auto data_string =
                    absl::StripAsciiWhitespace( data.child_value() );
                if( match( format, "ascii" ) )
                {
                    return read_ascii_data_array< T >( data_string );
                }
                return decode_base64_block< T >( data_string, type );
            }

            template < typename Out, typename In >
            std::vector< Out > cast_to( absl::Span< const In > values ) const
            {
                std::vector< Out > result( values.size() );
                for( const auto v : Indices{ values } )
                {
                    result[v] = static_cast< Out >( values[v] );
                }
                return result;
            }

            // Value marking elements without value: NaN for floating values
            // (as in VTK files), maximum value otherwise (NO_ID for index_t)
            template < typename T >
            static constexpr T missing_value()
            {
                if constexpr( std::is_floating_point_v< T > )
                {
                    return std::numeric_limits< T >::quiet_NaN();
                }
                else
                {
                    return std::numeric_limits< T >::max();
                }
            }

            template < typename T >
            void build_attribute( AttributeManager& manager,
                std::string_view name,
                absl::Span< const T > values,
                index_t nb_components,
                index_t offset )
            {
                OpenGeodeIOMeshException::check_exception(
                    values.size() % nb_components == 0, nullptr,
                    OpenGeodeException::TYPE::data,
                    "[VTKInput::build_attribute] Number of attribute "
                    "values is not a multiple of number of components" );
                if( nb_components == 1 )
                {
                    AttributeValues< T > default_values;
                    default_values.default_value = T{};
                    default_values.no_value = missing_value< T >();
                    AttributeProperties properties;
                    properties.assignable = false;
                    properties.interpolable = false;
                    properties.transferable = true;
                    auto attribute_id =
                        manager.create_attribute< VariableAttribute, T >(
                            name, default_values, properties );
                    auto attribute =
                        manager.find_attribute< VariableAttribute, T >(
                            attribute_id );
                    for( const auto i : Indices{ values } )
                    {
                        attribute->set_value( i + offset, values[i] );
                    }
                }
                else if( nb_components == 2 )
                {
                    std::array< T, 2 > no_value;
                    no_value.fill( missing_value< T >() );
                    create_attribute< std::array< T, 2 >, T >( manager, {},
                        no_value, values, nb_components, name, offset );
                }
                else if( nb_components == 3 )
                {
                    std::array< T, 3 > no_value;
                    no_value.fill( missing_value< T >() );
                    create_attribute< std::array< T, 3 >, T >( manager, {},
                        no_value, values, nb_components, name, offset );
                }
                else
                {
                    create_attribute< std::vector< T >, T >( manager,
                        std::vector< T >( nb_components ),
                        std::vector< T >( nb_components, missing_value< T >() ),
                        values, nb_components, name, offset );
                }
            }

            bool is_integer_type( std::string_view type ) const
            {
                for( const auto integer_type : { "Int8", "UInt8", "Int16",
                         "UInt16", "Int32", "UInt32", "Int64", "UInt64" } )
                {
                    if( match( type, integer_type ) )
                    {
                        return true;
                    }
                }
                return false;
            }

            // Stores integer values in the smallest attribute type that
            // holds them all
            void build_integer_attribute( AttributeManager& manager,
                std::string_view name,
                absl::Span< const int64_t > values,
                index_t nb_components,
                index_t offset )
            {
                const auto [min, max] = absl::c_minmax_element( values );
                if( values.empty()
                    || ( *min >= 0
                         && *max <= std::numeric_limits< index_t >::max() ) )
                {
                    build_attribute< index_t >( manager, name,
                        cast_to< index_t >( values ), nb_components, offset );
                }
                else if( *min >= std::numeric_limits< int >::lowest()
                         && *max <= std::numeric_limits< int >::max() )
                {
                    build_attribute< int >( manager, name,
                        cast_to< int >( values ), nb_components, offset );
                }
                else
                {
                    build_attribute< long int >( manager, name,
                        cast_to< long int >( values ), nb_components, offset );
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
                        read_data_array< double >( data );
                    build_attribute< double >( attribute_manager,
                        data_array_name, attribute_values, nb_components,
                        offset );
                }
                else if( is_integer_type( data_array_type ) )
                {
                    const auto attribute_values =
                        read_data_array< int64_t >( data );
                    build_integer_attribute( attribute_manager, data_array_name,
                        attribute_values, nb_components, offset );
                }
                else
                {
                    throw OpenGeodeIOMeshException{ nullptr,
                        OpenGeodeException::TYPE::internal,
                        "[VTKInput::read_data] Attribute of type ",
                        data_array_type, " is not supported" };
                }
            }

            void read_data( const pugi::xml_node& point_data,
                index_t offset,
                AttributeManager& attribute_manager )
            {
                for( const auto& data : point_data.children( "DataArray" ) )
                {
                    read_attribute_data( data, offset, attribute_manager );
                }
            }

            std::string_view read_appended_data(
                const pugi::xml_node& data ) const
            {
                const auto offset = data.attribute( "offset" ).as_ullong();
                return appended_data_.substr( offset );
            }

        private:
            template < typename Container, typename T >
            void create_attribute( AttributeManager& manager,
                const Container& default_value,
                const Container& no_value,
                absl::Span< const T > values,
                index_t nb_components,
                std::string_view name,
                index_t offset )
            {
                AttributeValues< Container > default_values;
                default_values.default_value = default_value;
                default_values.no_value = no_value;
                AttributeProperties properties;
                properties.assignable = false;
                properties.interpolable = false;
                properties.transferable = true;
                const auto attribute_id =
                    manager.create_attribute< VariableAttribute, Container >(
                        name, default_values, properties );
                auto attribute =
                    manager.find_attribute< VariableAttribute, Container >(
                        attribute_id );
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
                OpenGeodeIOMeshException::check_exception(
                    match( root_.attribute( "type" ).value(), type_ ), nullptr,
                    OpenGeodeException::TYPE::data,
                    "[VTKInput::read_root_attributes] VTK File type should be ",
                    type_ );
                little_endian_ = match(
                    root_.attribute( "byte_order" ).value(), "LittleEndian" );
                OpenGeodeIOMeshException::check_exception( little_endian_,
                    nullptr, OpenGeodeException::TYPE::internal,
                    "[VTKInput::read_root_attributes] Big Endian not "
                    "supported" );
                const auto compressor = root_.attribute( "compressor" ).value();
                OpenGeodeIOMeshException::check_exception(
                    std::string_view( compressor ).empty()
                        || match( compressor, "vtkZLibDataCompressor" ),
                    nullptr, OpenGeodeException::TYPE::internal,
                    "[VTKInput::read_root_attributes] Only "
                    "vtkZLibDataCompressor is supported for now" );
                compressed_ = !std::string_view( compressor ).empty();

                if( const auto header_type = root_.attribute( "header_type" ) )
                {
                    const auto& header_type_value = header_type.value();
                    OpenGeodeIOMeshException::check_exception(
                        match( header_type_value, "UInt32" )
                            || match( header_type_value, "UInt64" ),
                        nullptr, OpenGeodeException::TYPE::internal,
                        "[VTKInput::read_root_attributes] Cannot read VTKFile "
                        "with header_type ",
                        header_type_value,
                        ". Only UInt32 and Uint64 are accepted" );
                    is_uint64_ = match( header_type_value, "UInt64" );
                }
            }

            pugi::xml_parse_result parse_xml()
            {
                if( !extract_raw_appended_data() )
                {
                    return document_.load_buffer_inplace(
                        file_content_.data(), file_content_.size() );
                }
                // Raw appended data is not valid XML: it is kept aside and
                // only the remaining XML is given to the parser
                const std::string_view content{ file_content_ };
                const auto data_begin = static_cast< size_t >(
                    raw_appended_data_.data() - content.data() );
                const auto data_end = data_begin + raw_appended_data_.size();
                const auto xml = absl::StrCat( content.substr( 0, data_begin ),
                    content.substr( data_end ) );
                return document_.load_buffer( xml.data(), xml.size() );
            }

            bool extract_raw_appended_data()
            {
                const std::string_view content{ file_content_ };
                const auto tag_begin = content.find( "<AppendedData" );
                if( tag_begin == std::string_view::npos )
                {
                    return false;
                }
                const auto tag_end = content.find( '>', tag_begin );
                if( tag_end == std::string_view::npos )
                {
                    return false;
                }
                const auto tag =
                    content.substr( tag_begin, tag_end - tag_begin );
                if( !absl::StrContains( tag, "\"raw\"" )
                    && !absl::StrContains( tag, "'raw'" ) )
                {
                    return false;
                }
                const auto data_begin = content.find( '_', tag_end );
                const auto data_end = content.rfind( "</AppendedData>" );
                OpenGeodeIOMeshException::check_exception(
                    data_begin != std::string_view::npos
                        && data_end != std::string_view::npos
                        && data_begin < data_end,
                    nullptr, OpenGeodeException::TYPE::data,
                    "[VTKInput::extract_raw_appended_data] Malformed raw "
                    "AppendedData section" );
                // skip first char: '_'
                raw_appended_data_ =
                    content.substr( data_begin + 1, data_end - data_begin - 1 );
                return true;
            }

            void read_appended_data()
            {
                const auto node = root_.child( "AppendedData" );
                if( !node )
                {
                    return;
                }
                const auto encoding = node.attribute( "encoding" ).value();
                if( match( encoding, "raw" ) )
                {
                    is_raw_ = true;
                    appended_data_ = raw_appended_data_;
                    return;
                }
                OpenGeodeIOMeshException::check_exception(
                    match( encoding, "base64" ), nullptr,
                    OpenGeodeException::TYPE::data,
                    "[VTKInput::read_appended_data] VTK AppendedData "
                    "encoding should be raw or base64" );
                appended_data_ = node.child_value();
                appended_data_ = absl::StripAsciiWhitespace( appended_data_ );
                appended_data_.remove_prefix( 1 ); // skip first char: '_'
            }

            virtual void read_vtk_object(
                const pugi::xml_node& vtk_object ) = 0;

            template < typename T >
            std::vector< T > decode_base64_block(
                std::string_view input, std::string_view type ) const
            {
                if( is_uint64_ )
                {
                    return decode_raw_block< T >(
                        base64_to_raw_block< uint64_t >( input ), type );
                }
                return decode_raw_block< T >(
                    base64_to_raw_block< uint32_t >( input ), type );
            }

            template < typename T >
            std::vector< T > decode_raw_block(
                std::string_view input, std::string_view type ) const
            {
                std::string storage;
                if( is_uint64_ )
                {
                    return convert_bytes< T >(
                        raw_block_data< uint64_t >( input, storage ), type );
                }
                return convert_bytes< T >(
                    raw_block_data< uint32_t >( input, storage ), type );
            }

            static constexpr size_t base64_length( size_t nb_bytes )
            {
                return 4 * ( ( nb_bytes + 2 ) / 3 );
            }

            // Converts a base64 block into the raw block layout:
            // header followed by (possibly compressed) data
            template < typename UInt >
            std::string base64_to_raw_block( std::string_view input ) const
            {
                if( !compressed_ )
                {
                    // Header and data are encoded together
                    const auto header = decode_base64(
                        input.substr( 0, base64_length( sizeof( UInt ) ) ) );
                    const auto nb_bytes = read_raw_value< UInt >( header, 0 );
                    return decode_base64( input.substr(
                        0, base64_length( sizeof( UInt ) + nb_bytes ) ) );
                }
                // Header and data are encoded separately
                const auto fixed_header = decode_base64(
                    input.substr( 0, base64_length( 3 * sizeof( UInt ) ) ) );
                const auto nb_blocks =
                    read_raw_value< UInt >( fixed_header, 0 );
                const auto header_length =
                    base64_length( ( 3 + nb_blocks ) * sizeof( UInt ) );
                auto raw_block =
                    decode_base64( input.substr( 0, header_length ) );
                size_t compressed_size{ 0 };
                for( const auto b : Range{ nb_blocks } )
                {
                    compressed_size += read_raw_value< UInt >(
                        raw_block, ( 3 + b ) * sizeof( UInt ) );
                }
                raw_block += decode_base64( input.substr(
                    header_length, base64_length( compressed_size ) ) );
                return raw_block;
            }

            template < typename UInt >
            UInt read_raw_value( std::string_view input, size_t position ) const
            {
                OpenGeodeIOMeshException::check_exception(
                    position + sizeof( UInt ) <= input.size(), nullptr,
                    OpenGeodeException::TYPE::data,
                    "[VTKInput::read_raw_value] Unexpected end of data block" );
                UInt value;
                std::memcpy( &value, input.data() + position, sizeof( UInt ) );
                return value;
            }

            // Returns the uncompressed bytes of a raw block, either viewing
            // the input or stored in the given storage after decompression
            template < typename UInt >
            std::string_view raw_block_data(
                std::string_view input, std::string& storage ) const
            {
                if( !compressed_ )
                {
                    const auto nb_bytes = read_raw_value< UInt >( input, 0 );
                    OpenGeodeIOMeshException::check_exception(
                        sizeof( UInt ) + nb_bytes <= input.size(), nullptr,
                        OpenGeodeException::TYPE::data,
                        "[VTKInput::raw_block_data] Unexpected end of data "
                        "block" );
                    return input.substr( sizeof( UInt ), nb_bytes );
                }
                // Header: [nb_blocks, block_size, last_block_size,
                // compressed_size_0, ..., compressed_size_{nb_blocks-1}]
                const auto nb_blocks = read_raw_value< UInt >( input, 0 );
                if( nb_blocks == 0 )
                {
                    return {};
                }
                const auto block_size =
                    read_raw_value< UInt >( input, sizeof( UInt ) );
                const auto last_block_size =
                    read_raw_value< UInt >( input, 2 * sizeof( UInt ) );
                const auto last_size =
                    last_block_size == 0 ? block_size : last_block_size;
                storage.resize( ( nb_blocks - 1 ) * block_size + last_size );
                auto* output = reinterpret_cast< Bytef* >( storage.data() );
                auto data_position = ( 3 + nb_blocks ) * sizeof( UInt );
                for( const auto b : Range{ nb_blocks } )
                {
                    const auto compressed_size = read_raw_value< UInt >(
                        input, ( 3 + b ) * sizeof( UInt ) );
                    OpenGeodeIOMeshException::check_exception(
                        data_position + compressed_size <= input.size(),
                        nullptr, OpenGeodeException::TYPE::data,
                        "[VTKInput::raw_block_data] Unexpected end of data "
                        "block" );
                    const auto expected_size =
                        b + 1 == nb_blocks ? last_size : block_size;
                    uLongf uncompressed_size = expected_size;
                    const auto uncompress_result =
                        uncompress( output + b * block_size, &uncompressed_size,
                            reinterpret_cast< const Bytef* >(
                                input.data() + data_position ),
                            compressed_size );
                    OpenGeodeIOMeshException::check_exception(
                        uncompress_result == Z_OK
                            && uncompressed_size == expected_size,
                        nullptr, OpenGeodeException::TYPE::data,
                        "[VTKInput::raw_block_data] Error in zlib "
                        "decompressing data" );
                    data_position += compressed_size;
                }
                return storage;
            }

            // Converts bytes stored with the given VTK data type into T values
            template < typename T >
            std::vector< T > convert_bytes(
                std::string_view bytes, std::string_view type ) const
            {
                if( match( type, "Float64" ) )
                {
                    return cast_bytes< T, double >( bytes );
                }
                if( match( type, "Float32" ) )
                {
                    return cast_bytes< T, float >( bytes );
                }
                if( match( type, "Int64" ) )
                {
                    return cast_bytes< T, int64_t >( bytes );
                }
                if( match( type, "UInt64" ) )
                {
                    return cast_bytes< T, uint64_t >( bytes );
                }
                if( match( type, "Int32" ) )
                {
                    return cast_bytes< T, int32_t >( bytes );
                }
                if( match( type, "UInt32" ) )
                {
                    return cast_bytes< T, uint32_t >( bytes );
                }
                if( match( type, "Int16" ) )
                {
                    return cast_bytes< T, int16_t >( bytes );
                }
                if( match( type, "UInt16" ) )
                {
                    return cast_bytes< T, uint16_t >( bytes );
                }
                if( match( type, "Int8" ) )
                {
                    return cast_bytes< T, int8_t >( bytes );
                }
                if( match( type, "UInt8" ) )
                {
                    return cast_bytes< T, uint8_t >( bytes );
                }
                throw OpenGeodeIOMeshException{ nullptr,
                    OpenGeodeException::TYPE::data,
                    "[VTKInput::convert_bytes] Data type ", type,
                    " is not supported" };
            }

            template < typename T, typename Stored >
            std::vector< T > cast_bytes( std::string_view bytes ) const
            {
                std::vector< T > result( bytes.size() / sizeof( Stored ) );
                if( result.empty() )
                {
                    return result;
                }
                if constexpr( std::is_same_v< T, Stored > )
                {
                    std::memcpy( result.data(), bytes.data(),
                        result.size() * sizeof( T ) );
                }
                else
                {
                    for( const auto i : Indices{ result } )
                    {
                        Stored value;
                        std::memcpy( &value,
                            bytes.data() + i * sizeof( Stored ),
                            sizeof( Stored ) );
                        result[i] = static_cast< T >( value );
                    }
                }
                return result;
            }

            std::string decode_base64( std::string_view input ) const
            {
                std::string bytes;
                auto decode_status = absl::Base64Unescape( input, &bytes );
                OpenGeodeIOMeshException::check_exception( decode_status,
                    nullptr, OpenGeodeException::TYPE::data,
                    "[VTKInput::decode_base64] Error in decoding base64 data" );
                return bytes;
            }

            template < typename T >
            std::vector< T > read_ascii_data_array(
                std::string_view data ) const
            {
                std::vector< T > results;
                size_t begin{ 0 };
                while( true )
                {
                    while( begin < data.size()
                           && absl::ascii_isspace( data[begin] ) )
                    {
                        begin++;
                    }
                    if( begin == data.size() )
                    {
                        break;
                    }
                    auto end = begin;
                    while(
                        end < data.size() && !absl::ascii_isspace( data[end] ) )
                    {
                        end++;
                    }
                    results.push_back( read_ascii_value< T >(
                        data.substr( begin, end - begin ) ) );
                    begin = end;
                }
                return results;
            }

            template < typename T >
            T read_ascii_value( std::string_view string ) const
            {
                using Parsed =
                    std::conditional_t< std::is_floating_point_v< T >, double,
                        int64_t >;
                Parsed value;
                bool ok;
                if constexpr( std::is_floating_point_v< T > )
                {
                    ok = absl::SimpleAtod( string, &value );
                }
                else
                {
                    ok = absl::SimpleAtoi( string, &value );
                }
                OpenGeodeIOMeshException::check_exception( ok, nullptr,
                    OpenGeodeException::TYPE::data,
                    "[VTKInput::read_ascii_value] Failed to read value: ",
                    string );
                return static_cast< T >( value );
            }

        private:
            std::ifstream file_;
            std::unique_ptr< Mesh > mesh_;
            pugi::xml_document document_;
            pugi::xml_node root_;
            const char* type_;
            bool little_endian_{ true };
            bool compressed_{ false };
            bool is_uint64_{ false };
            bool is_raw_{ false };
            std::string file_content_;
            std::string_view raw_appended_data_;
            std::string_view appended_data_;
        }; // namespace detail
    } // namespace detail
} // namespace geode
