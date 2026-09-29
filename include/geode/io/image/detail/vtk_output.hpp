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

#include <geode/io/image/common.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

#include <pugixml.hpp>

#include <absl/algorithm/container.h>

#include <geode/basic/attribute_manager.hpp>

#include <geode/geometry/point.hpp>
#include <geode/geometry/vector.hpp>

namespace geode
{
    namespace detail
    {
        template < typename T >
        constexpr const char* vtk_data_type()
        {
            static_assert(
                std::is_arithmetic_v< T > && !std::is_same_v< T, bool >,
                "[vtk_data_type] Type not supported by VTK" );
            if constexpr( std::is_floating_point_v< T > )
            {
                return sizeof( T ) == 4 ? "Float32" : "Float64";
            }
            else if constexpr( std::is_signed_v< T > )
            {
                constexpr std::array< const char*, 9 > names{ "", "Int8",
                    "Int16", "", "Int32", "", "", "", "Int64" };
                return names[sizeof( T )];
            }
            else
            {
                constexpr std::array< const char*, 9 > names{ "", "UInt8",
                    "UInt16", "", "UInt32", "", "", "", "UInt64" };
                return names[sizeof( T )];
            }
        }

        // How an attribute value type is stored in a VTK DataArray
        template < typename Value, typename = void >
        struct VTKAttributeValue;

        template < typename Value >
        struct VTKAttributeValue< Value,
            std::enable_if_t< std::is_arithmetic_v< Value > > >
        {
            using Stored = std::
                conditional_t< std::is_same_v< Value, bool >, uint8_t, Value >;
            static constexpr local_index_t nb_components = 1;
            static Stored component(
                const Value& value, local_index_t /*unused*/ )
            {
                return value;
            }
        };

        template < typename Value, size_t size >
        struct VTKAttributeValue< std::array< Value, size > >
        {
            using Stored = typename VTKAttributeValue< Value >::Stored;
            static constexpr local_index_t nb_components = size;
            static Stored component(
                const std::array< Value, size >& value, local_index_t c )
            {
                return value[c];
            }
        };

        template < index_t dimension >
        struct VTKAttributeValue< Point< dimension > >
        {
            using Stored = double;
            static constexpr local_index_t nb_components = dimension;
            static Stored component(
                const Point< dimension >& value, local_index_t c )
            {
                return value.value( c );
            }
        };

        template < index_t dimension >
        struct VTKAttributeValue< Vector< dimension > >
        {
            using Stored = double;
            static constexpr local_index_t nb_components = dimension;
            static Stored component(
                const Vector< dimension >& value, local_index_t c )
            {
                return value.value( c );
            }
        };

        template < typename... Values >
        struct VTKAttributeTypeList
        {
        };

        template < typename Value >
        using VTKAttributeArrays = VTKAttributeTypeList< std::array< Value, 2 >,
            std::array< Value, 3 >,
            std::array< Value, 4 > >;

        // Attribute value types written with their own VTK type, other
        // genericable attributes are written as Float32
        using VTKScalarAttributeTypes = VTKAttributeTypeList< bool,
            signed char,
            unsigned char,
            short,
            unsigned short,
            int,
            unsigned int,
            long,
            unsigned long,
            long long,
            unsigned long long,
            float,
            double,
            Point2D,
            Point3D,
            Vector2D,
            Vector3D >;

        template < typename Mesh >
        class VTKOutputImpl
        {
            // Each appended data block starts with its size in bytes
            using HeaderType = uint64_t;

        public:
            void write_file()
            {
                auto root = write_root_attributes();
                write_vtk_object( root );
                if( appended_data_.empty() )
                {
                    document_.save( file_ );
                    return;
                }
                auto appended = root.append_child( "AppendedData" );
                appended.append_attribute( "encoding" ).set_value( "raw" );
                appended.text().set( "_" );
                std::ostringstream xml;
                document_.save( xml );
                const auto xml_string = xml.str();
                const auto split = xml_string.rfind( "_</AppendedData>" ) + 1;
                file_.write( xml_string.data(), split );
                file_.write( appended_data_.data(), appended_data_.size() );
                file_.write(
                    xml_string.data() + split, xml_string.size() - split );
            }

        protected:
            VTKOutputImpl(
                std::string_view filename, const Mesh& mesh, const char* type )
                : filename_{ filename },
                  file_{ to_string( filename ), std::ios::binary },
                  mesh_( mesh ),
                  type_{ type }
            {
                OpenGeodeIOImageException::check_exception( file_.good(),
                    nullptr, OpenGeodeException::TYPE::data,
                    "[VTKOutput] Error while writing file: ", filename );
            }

            virtual ~VTKOutputImpl() {}

            const Mesh& mesh() const
            {
                return mesh_;
            }

            std::string_view filename() const
            {
                return filename_;
            }

            void write_attributes( pugi::xml_node& attribute_node,
                const AttributeManager& manager )
            {
                absl::FixedArray< index_t > elements( manager.nb_elements() );
                absl::c_iota( elements, 0 );
                write_attributes( attribute_node, manager, elements );
            }

            void write_attributes( pugi::xml_node& attribute_node,
                const AttributeManager& manager,
                absl::Span< const index_t > elements )
            {
                for( const auto& id : manager.attribute_ids() )
                {
                    const auto attribute = manager.find_generic_attribute( id );
                    if( !attribute || !attribute->properties().transferable )
                    {
                        continue;
                    }
                    if( write_typed_attribute( attribute_node, *attribute,
                            elements, VTKScalarAttributeTypes{} )
                        || write_typed_attribute( attribute_node, *attribute,
                            elements, VTKAttributeArrays< bool >{} )
                        || write_typed_attribute( attribute_node, *attribute,
                            elements, VTKAttributeArrays< unsigned char >{} )
                        || write_typed_attribute( attribute_node, *attribute,
                            elements, VTKAttributeArrays< int >{} )
                        || write_typed_attribute( attribute_node, *attribute,
                            elements, VTKAttributeArrays< unsigned int >{} )
                        || write_typed_attribute( attribute_node, *attribute,
                            elements, VTKAttributeArrays< float >{} )
                        || write_typed_attribute( attribute_node, *attribute,
                            elements, VTKAttributeArrays< double >{} ) )
                    {
                        continue;
                    }
                    if( attribute->is_genericable() )
                    {
                        write_generic_attribute(
                            attribute_node, *attribute, elements );
                    }
                }
            }

            // Writes a DataArray with its RangeMin/RangeMax computed over all
            // the values (NaN values are ignored)
            template < typename T >
            pugi::xml_node write_data_array( pugi::xml_node& parent,
                std::string_view name,
                absl::Span< const T > values,
                local_index_t nb_components = 1 )
            {
                auto data_array = parent.append_child( "DataArray" );
                data_array.append_attribute( "type" ).set_value(
                    vtk_data_type< T >() );
                data_array.append_attribute( "Name" ).set_value(
                    to_string( name ).c_str() );
                data_array.append_attribute( "format" ).set_value( "appended" );
                data_array.append_attribute( "NumberOfComponents" )
                    .set_value( nb_components );
                write_range( data_array, values );
                data_array.append_attribute( "offset" )
                    .set_value( appended_data_.size() );
                const HeaderType nb_bytes = values.size() * sizeof( T );
                const auto old_size = appended_data_.size();
                appended_data_.resize(
                    old_size + sizeof( HeaderType ) + nb_bytes );
                auto* data = appended_data_.data() + old_size;
                std::memcpy( data, &nb_bytes, sizeof( HeaderType ) );
                if( nb_bytes > 0 )
                {
                    std::memcpy(
                        data + sizeof( HeaderType ), values.data(), nb_bytes );
                }
                return data_array;
            }

        private:
            template < typename... Values >
            bool write_typed_attribute( pugi::xml_node& attribute_node,
                AttributeBase& attribute,
                absl::Span< const index_t > elements,
                VTKAttributeTypeList< Values... > /*unused*/ )
            {
                return ( write_typed_attribute< Values >(
                             attribute_node, attribute, elements )
                         || ... );
            }

            template < typename Value >
            bool write_typed_attribute( pugi::xml_node& attribute_node,
                AttributeBase& attribute,
                absl::Span< const index_t > elements )
            {
                const auto* typed =
                    dynamic_cast< const ReadOnlyAttribute< Value >* >(
                        &attribute );
                if( !typed )
                {
                    return false;
                }
                using Stored = typename VTKAttributeValue< Value >::Stored;
                if constexpr( !std::is_floating_point_v< Stored > )
                {
                    // VTK marks missing values with NaN, only available in
                    // floating arrays
                    const auto has_all_values =
                        absl::c_all_of( elements, [typed]( index_t e ) {
                            return typed->has_value( e );
                        } );
                    if( !has_all_values )
                    {
                        write_typed_values< Value, double >(
                            attribute_node, *typed, elements );
                        return true;
                    }
                }
                write_typed_values< Value, Stored >(
                    attribute_node, *typed, elements );
                return true;
            }

            template < typename Value, typename Stored >
            void write_typed_values( pugi::xml_node& attribute_node,
                const ReadOnlyAttribute< Value >& attribute,
                absl::Span< const index_t > elements )
            {
                using Traits = VTKAttributeValue< Value >;
                std::vector< Stored > values;
                values.reserve( elements.size() * Traits::nb_components );
                for( const auto e : elements )
                {
                    if constexpr( std::is_floating_point_v< Stored > )
                    {
                        if( !attribute.has_value( e ) )
                        {
                            values.insert( values.end(), Traits::nb_components,
                                std::numeric_limits< Stored >::quiet_NaN() );
                            continue;
                        }
                    }
                    const auto& value = attribute.value( e );
                    for( const auto c : LRange{ Traits::nb_components } )
                    {
                        values.push_back( static_cast< Stored >(
                            Traits::component( value, c ) ) );
                    }
                }
                write_data_array< Stored >( attribute_node,
                    attribute.name().value(), values, Traits::nb_components );
            }

            void write_generic_attribute( pugi::xml_node& attribute_node,
                const AttributeBase& attribute,
                absl::Span< const index_t > elements )
            {
                const auto nb_items = attribute.nb_items();
                std::vector< float > values;
                values.reserve( elements.size() * nb_items );
                for( const auto e : elements )
                {
                    const auto has_value = attribute.has_value( e );
                    for( const auto i : LRange{ nb_items } )
                    {
                        values.push_back(
                            has_value ? attribute.generic_item_value( e, i )
                                      : std::nanf( "" ) );
                    }
                }
                write_data_array< float >( attribute_node,
                    attribute.name().value(), values, nb_items );
            }

            template < typename T >
            static void write_range(
                pugi::xml_node& data_array, absl::Span< const T > values )
            {
                auto min = std::numeric_limits< T >::max();
                auto max = std::numeric_limits< T >::lowest();
                for( const auto value : values )
                {
                    if constexpr( std::is_floating_point_v< T > )
                    {
                        if( std::isnan( value ) )
                        {
                            continue;
                        }
                    }
                    min = std::min( min, value );
                    max = std::max( max, value );
                }
                if( min > max )
                {
                    return;
                }
                if constexpr( std::is_floating_point_v< T > )
                {
                    data_array.append_attribute( "RangeMin" )
                        .set_value( static_cast< double >( min ) );
                    data_array.append_attribute( "RangeMax" )
                        .set_value( static_cast< double >( max ) );
                }
                else if constexpr( std::is_signed_v< T > )
                {
                    data_array.append_attribute( "RangeMin" )
                        .set_value( static_cast< long long >( min ) );
                    data_array.append_attribute( "RangeMax" )
                        .set_value( static_cast< long long >( max ) );
                }
                else
                {
                    data_array.append_attribute( "RangeMin" )
                        .set_value( static_cast< unsigned long long >( min ) );
                    data_array.append_attribute( "RangeMax" )
                        .set_value( static_cast< unsigned long long >( max ) );
                }
            }

            pugi::xml_node write_root_attributes()
            {
                auto root = document_.append_child( "VTKFile" );
                root.append_attribute( "type" ).set_value( type_ );
                root.append_attribute( "version" ).set_value( "1.0" );
                root.append_attribute( "byte_order" )
                    .set_value( "LittleEndian" );
                root.append_attribute( "header_type" ).set_value( "UInt64" );
                return root;
            }

            void write_vtk_object( pugi::xml_node& root )
            {
                auto object = root.append_child( type_ );
                write_piece( object );
            }

            virtual void write_piece( pugi::xml_node& object ) = 0;

        private:
            std::string_view filename_;
            std::ofstream file_;
            const Mesh& mesh_;
            pugi::xml_document document_;
            const char* type_;
            std::string appended_data_;
        };
    } // namespace detail
} // namespace geode
