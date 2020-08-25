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

#include <geode/io/mesh/private/vtp_output.h>

#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/core/polygonal_surface.h>

#include <geode/io/mesh/private/vtk_output.h>

namespace
{
    class VTPOutputImpl
        : public geode::detail::VTKOutputImpl< geode::PolygonalSurface3D,
              geode::PolygonalSurfaceBuilder3D >
    {
    public:
        VTPOutputImpl( absl::string_view filename,
            const geode::PolygonalSurface3D& polygonal_surface )
            : geode::detail::VTKOutputImpl< geode::PolygonalSurface3D,
                geode::PolygonalSurfaceBuilder3D >(
                filename, polygonal_surface, "PolyData" )
        {
        }

    private:
        void append_number_elements( pugi::xml_node& piece ) override
        {
            piece.append_attribute( "NumberOfPolys" )
                .set_value( mesh().nb_polygons() );
        }

        void write_vtk_cells( pugi::xml_node& piece ) override
        {
            auto polys = piece.append_child( "Polys" );
            auto connectivity = polys.append_child( "DataArray" );
            connectivity.append_attribute( "type" ).set_value( "Int64" );
            connectivity.append_attribute( "Name" ).set_value( "connectivity" );
            connectivity.append_attribute( "format" ).set_value( "binary" );
            connectivity.append_attribute( "RangeMin" ).set_value( 0 );
            connectivity.append_attribute( "RangeMax" )
                .set_value( mesh().nb_vertices() - 1 );
            auto offsets = polys.append_child( "DataArray" );
            offsets.append_attribute( "type" ).set_value( "Int64" );
            offsets.append_attribute( "Name" ).set_value( "offsets" );
            offsets.append_attribute( "format" ).set_value( "ascii" );
            offsets.append_attribute( "RangeMin" ).set_value( 0 );
            offsets.append_attribute( "RangeMax" )
                .set_value( mesh().nb_vertices() );
            std::string poly_connectivity;
            std::string poly_offsets;
            geode::index_t vertex_count{ 0 };
            std::vector< geode::index_t > truc;
            for( const auto p : geode::Range{ mesh().nb_polygons() } )
            {
                const auto nb_polygon_vertices =
                    mesh().nb_polygon_vertices( p );
                vertex_count += nb_polygon_vertices;
                poly_offsets += std::to_string( vertex_count );
                poly_offsets += " ";
                for( const auto v : geode::Range{ nb_polygon_vertices } )
                {
                    poly_connectivity +=
                        std::to_string( mesh().polygon_vertex( { p, v } ) );
                    poly_connectivity += " ";
                    truc.push_back( mesh().polygon_vertex( { p, v } ) );
                    // DEBUG( truc.back() );
                }
            }
            connectivity.text().set( poly_connectivity.c_str() );
            offsets.text().set( poly_offsets.c_str() );
            DEBUG( truc.size() );
            DEBUG( truc.size() * 4 );
            const auto value_size = sizeof( geode::index_t );
            const auto data_size = truc.size() * value_size;

            // header
            constexpr double max_block_size = std::pow( 2, 17 );
            DEBUG( max_block_size );
            const auto max_nb_value_per_block =
                std::floor( max_block_size / value_size );
            DEBUG( value_size );
            DEBUG( max_nb_value_per_block );

            const geode::index_t nb_blocks =
                std::ceil( truc.size() / max_nb_value_per_block );
            const geode::index_t nb_value_per_block = max_nb_value_per_block;
            DEBUG( nb_blocks );
            const geode::index_t block_size_before_compression =
                value_size * nb_value_per_block;
            geode::index_t last_partial_block_size_before_compression{ 0 };
            if( nb_blocks > 1 )
            {
                last_partial_block_size_before_compression =
                    data_size
                    - block_size_before_compression * ( nb_blocks - 1 );
            }
            DEBUG( nb_blocks );
            DEBUG( block_size_before_compression );
            DEBUG( last_partial_block_size_before_compression );

            std::vector< geode::index_t > header_init{ nb_blocks,
                block_size_before_compression,
                last_partial_block_size_before_compression };
            header_init.reserve( 3 + nb_blocks );

            // blocks compression
            std::string data_string;
            const auto truc_good =
                reinterpret_cast< const uint8_t* >( truc.data() );
            size_t result_size;
            for( const auto b : geode::Range{ nb_blocks - 1 } )
            {
                DEBUG( "=====" );
                DEBUG( result_size );
                // values_span
                DEBUG( b * nb_value_per_block );
                DEBUG( nb_value_per_block * value_size );
                const auto compress_bound_size =
                    zng_compressBound( block_size_before_compression );
                DEBUG( compress_bound_size );
                std::vector< uint8_t > compressed_data_bytes_init(
                    compress_bound_size );
                const auto compress_result =
                    zng_compress( compressed_data_bytes_init.data(),
                        &result_size, &truc_good[b * nb_value_per_block],
                        nb_value_per_block * value_size );
                DEBUG( compress_result == Z_OK );
                DEBUG( compress_result == Z_MEM_ERROR );
                DEBUG( compress_result == Z_BUF_ERROR );
                DEBUG( compress_result == Z_STREAM_ERROR );
                DEBUG( compress_result );
                DEBUG( result_size );
                const auto mc = reinterpret_cast< const char* >(
                    compressed_data_bytes_init.data() );
                auto machin = absl::Base64Escape( mc );
                DEBUG( machin );
                data_string += machin;
                header_init.push_back( result_size );
            }

            DEBUG( "=====~~~~~+====" );
            // values_span
            DEBUG( truc.size() );
            DEBUG( nb_blocks * nb_value_per_block );
            DEBUG( truc.size() - nb_blocks * nb_value_per_block );
            // const auto truc_good = reinterpret_cast< const uint8_t* >(
            //     &truc[nb_blocks * max_nb_value_per_block] );
            // size_t result_size;
            DEBUG( result_size );
            const auto compress_bound_size =
                zng_compressBound( last_partial_block_size_before_compression );
            DEBUG( compress_bound_size );
            std::vector< uint8_t > compressed_data_bytes_init(
                compress_bound_size );
            const auto compress_result =
                zng_compress( compressed_data_bytes_init.data(), &result_size,
                    &truc_good[nb_blocks * nb_value_per_block],
                    data_size
                        - ( ( nb_blocks - 1 ) * max_nb_value_per_block )
                              * value_size );
            DEBUG( compress_result == Z_OK );
            DEBUG( compress_result == Z_MEM_ERROR );
            DEBUG( compress_result == Z_BUF_ERROR );
            DEBUG( compress_result == Z_STREAM_ERROR );
            DEBUG( compress_result );
            DEBUG( result_size );
            const auto mc = reinterpret_cast< const char* >(
                compressed_data_bytes_init.data() );
            auto machin = absl::Base64Escape( mc );
            DEBUG( machin );
            data_string += machin;
            header_init.push_back( result_size );

            geode::index_t block_size_after_compression = result_size;
            DEBUG( header_init[0] );
            DEBUG( header_init[1] );
            DEBUG( header_init[2] );
            DEBUG( header_init[3] );
            DEBUG( header_init[4] );
            DEBUG( header_init[5] );
            DEBUG( header_init[6] );
            DEBUG( header_init[7] );
            DEBUG( header_init[8] );

            DEBUG( header_init.size() );
            DEBUG( sizeof( header_init ) );

            //...
            DEBUG( sizeof( header_init ) );
            auto bytes = reinterpret_cast< char* >( header_init.data() );

            const auto str64 = absl::Base64Escape(
                absl::string_view{ bytes, 4 * header_init.size() } );
            DEBUG( str64 );

            auto full = str64 + data_string;
            DEBUG( full );
            connectivity.text().set( full.c_str() );

            // const auto compressed_data_bytes =
            //     reinterpret_cast< const unsigned char* >( bytes.c_str() );
            // size_t decompressed_data_length = dest_len;
            // absl::FixedArray< uint8_t > decompressed_data_bytes(
            //     decompressed_data_length );
            // const auto uncompress_result = zng_uncompress(
            //     decompressed_data_bytes.data(), &decompressed_data_length,
            //     &compressed_data_bytes_init[0], result_size );
            // DEBUG( uncompress_result == Z_OK );
            // DEBUG( uncompress_result == Z_MEM_ERROR );
            // DEBUG( uncompress_result == Z_BUF_ERROR );
            // DEBUG( uncompress_result == Z_DATA_ERROR );
            // DEBUG( uncompress_result == Z_STREAM_ERROR );
            // DEBUG( uncompress_result );
            // OPENGEODE_EXCEPTION( uncompress_result == Z_OK,
            //     "[VTKInput::decode] Error in zlib decompressing data" );
            // const auto values = reinterpret_cast< const geode::index_t* >(
            //     decompressed_data_bytes.data() );
            // const auto nb_values =
            //     decompressed_data_length / sizeof( geode::index_t );
            // DEBUG( nb_values );
            // for( const auto v : geode::Range{ nb_values } )
            // {
            //     DEBUG( values[v] );
            // }
        }

        void write_vtk_cell_attributes( pugi::xml_node& piece ) override
        {
            auto cell_data = piece.append_child( "CellData" );
            const auto names =
                mesh().polygon_attribute_manager().attribute_names();
            for( const auto name : names )
            {
                const auto attribute =
                    mesh().polygon_attribute_manager().find_generic_attribute(
                        name );
                if( !attribute || !attribute->is_genericable() )
                {
                    continue;
                }
                auto data_array = cell_data.append_child( "DataArray" );
                data_array.append_attribute( "type" ).set_value( "Float64" );
                data_array.append_attribute( "Name" ).set_value( name.data() );
                data_array.append_attribute( "format" ).set_value( "ascii" );
                auto min = attribute->generic_value( 0 );
                auto max = attribute->generic_value( 0 );
                std::string values;
                for( const auto v : geode::Range{ mesh().nb_polygons() } )
                {
                    const auto value = attribute->generic_value( v );
                    values += std::to_string( value );
                    values += " ";
                    min = std::min( min, value );
                    max = std::max( max, value );
                }
                data_array.append_attribute( "RangeMin" ).set_value( min );
                data_array.append_attribute( "RangeMax" ).set_value( max );
                data_array.text().set( values.c_str() );
            }
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void VTPOutput::write() const
        {
            VTPOutputImpl impl{ filename(), polygonal_surface() };
            impl.write_file();
        }
    } // namespace detail
} // namespace geode
