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

#include <geode/geometry/point.h>

#include <geode/io/image/private/vtk_output.h>

namespace geode
{
    namespace detail
    {
        template < typename CellArray >
        class VTIOutputImpl : public detail::VTKOutputImpl< CellArray >
        {
            static constexpr auto dimension = CellArray::dim;

        protected:
            VTIOutputImpl( const CellArray& array, absl::string_view filename )
                : detail::VTKOutputImpl< CellArray >{ filename, array,
                      "ImageData" }
            {
            }

            void write_image_header( pugi::xml_node& piece,
                const Point< dimension >& origin,
                const std::array< index_t, dimension >& extent,
                const std::array< double, dimension >& spacing )
            {
                auto image = piece.parent();
                std::string extent_str;
                for( const auto d : LRange{ dimension } )
                {
                    if( d != 0 )
                    {
                        absl::StrAppend( &extent_str, " " );
                    }
                    absl::StrAppend( &extent_str, "0 ", extent[d] - 1 );
                }
                if( dimension == 2 )
                {
                    absl::StrAppend( &extent_str, " 0 0" );
                }
                image.append_attribute( "WholeExtent" )
                    .set_value( extent_str.c_str() );
                piece.append_attribute( "Extent" )
                    .set_value( extent_str.c_str() );
                std::string origin_str;
                absl::StrAppend( &origin_str, origin.string() );
                if( dimension == 2 )
                {
                    absl::StrAppend( &origin_str, " 0" );
                }
                image.append_attribute( "Origin" )
                    .set_value( origin_str.c_str() );
                std::string spacing_str;
                for( const auto d : LRange{ dimension } )
                {
                    if( d != 0 )
                    {
                        absl::StrAppend( &spacing_str, " " );
                    }
                    absl::StrAppend( &spacing_str, spacing[d] );
                }
                if( dimension == 2 )
                {
                    absl::StrAppend( &spacing_str, " 1" );
                }
                image.append_attribute( "Spacing" )
                    .set_value( spacing_str.c_str() );
            }
        };
    } // namespace detail
} // namespace geode
