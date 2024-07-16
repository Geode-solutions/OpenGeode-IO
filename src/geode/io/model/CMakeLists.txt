# Copyright (c) 2019 - 2024 Geode-solutions
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
    NAME model
    FOLDER "geode/io/model"
    SOURCES
        "common.cpp"
        "msh_input.cpp"
        "msh_output.cpp"
        "svg_input.cpp"
        "vtm_section_output.cpp"
        "vtm_brep_output.cpp"
    PUBLIC_HEADERS
        "common.hpp"
    ADVANCED_HEADERS
        "detail/vtm_output.hpp"
        "detail/vtm_section_output.hpp"
        "detail/vtm_brep_output.hpp"
    INTERNAL_HEADERS
        "internal/msh_input.hpp"
        "internal/msh_output.hpp"
        "internal/svg_input.hpp"
    PRIVATE_DEPENDENCIES
        Async++
        OpenGeode::basic
        OpenGeode::geometry
        OpenGeode::mesh
        OpenGeode::model
        pugixml::pugixml
        ZLIB::ZLIB
        ${PROJECT_NAME}::mesh
)
