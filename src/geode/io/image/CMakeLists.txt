# Copyright (c) 2019 - 2025 Geode-solutions
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

add_geode_library(
    NAME image
    FOLDER "geode/io/image"
    SOURCES
        "bmp_input.cpp"
        "common.cpp"
        "jpg_input.cpp"
        "png_input.cpp"
        "raster_image_input.cpp"
        "vti_raster_image_output.cpp"
    PUBLIC_HEADERS
        "common.hpp"
    ADVANCED_HEADERS
        "detail/vti_output_impl.hpp"
        "detail/vti_raster_image_output.hpp"
        "detail/vtk_output.hpp"
    INTERNAL_HEADERS
        "internal/bmp_input.hpp"
        "internal/jpg_input.hpp"
        "internal/png_input.hpp"
        "internal/raster_image_input.hpp"
    PUBLIC_DEPENDENCIES
        OpenGeode::image
    PRIVATE_DEPENDENCIES
        OpenGeode::basic
        OpenGeode::geometry
        pugixml::pugixml
        GDAL::GDAL
)
