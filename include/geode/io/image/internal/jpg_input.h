/*
 * Copyright (c) 2019 - 2024 Geode-solutions
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

#include <geode/image/io/raster_image_input.h>

namespace geode
{
    FORWARD_DECLARATION_DIMENSION_CLASS( RasterImage );
    ALIAS_2D( RasterImage );
} // namespace geode

namespace geode
{
    namespace internal
    {
        class JPGInput final : public RasterImageInput< 2 >
        {
        public:
            explicit JPGInput( std::string_view filename )
                : RasterImageInput< 2 >( filename )
            {
            }

            static std::string_view extension()
            {
                static constexpr auto EXT = "jpg";
                return EXT;
            }

            RasterImage2D read() final;
        };
    } // namespace internal
} // namespace geode