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

#include <geode/io/image/common.h>

#include <gdal_priv.h>

#include <geode/image/common.h>

#include <geode/io/image/private/bmp_input.h>
#include <geode/io/image/private/jpg_input.h>
#include <geode/io/image/private/png_input.h>
#include <geode/io/image/private/vti_raster_image_output.h>

namespace
{
    void register_raster_input()
    {
        geode::RasterImageInputFactory2D::register_creator<
            geode::detail::JPGInput >(
            geode::detail::JPGInput::extension().data() );

        geode::RasterImageInputFactory2D::register_creator<
            geode::detail::PNGInput >(
            geode::detail::PNGInput::extension().data() );

        geode::RasterImageInputFactory2D::register_creator<
            geode::detail::BMPInput >(
            geode::detail::BMPInput::extension().data() );
    }

    void register_raster_output()
    {
        geode::RasterImageOutputFactory2D::register_creator<
            geode::detail::VTIRasterImageOutput2D >(
            geode::detail::VTIRasterImageOutput2D::extension().data() );

        geode::RasterImageOutputFactory3D::register_creator<
            geode::detail::VTIRasterImageOutput3D >(
            geode::detail::VTIRasterImageOutput3D::extension().data() );
    }
} // namespace

namespace geode
{
    OPENGEODE_LIBRARY_IMPLEMENTATION( IOImage )
    {
        OpenGeodeImageLibrary::initialize();
        register_raster_input();
        register_raster_output();
        GDALAllRegister();
    }
} // namespace geode
