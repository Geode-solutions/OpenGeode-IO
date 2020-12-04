# Copyright (c) 2019 - 2020 Geode-solutions
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
    NAME mesh
    FOLDER "geode/io/mesh"
    SOURCES
        "assimp_input.cpp"
        "common.cpp"
        "dxf_input.cpp"
        "obj_input.cpp"
        "obj_polygonal_output.cpp"
        "obj_triangulated_output.cpp"
        "ply_input.cpp"
        "ply_output.cpp"
        "stl_input.cpp"
        "stl_output.cpp"
        "vtp_input.cpp"
        "vtp_polygonal_output.cpp"
        "vtp_triangulated_output.cpp"
        "vtu_input.cpp"
        "vtu_output.cpp"
    ADVANCED_HEADERS
        "detail/common.h"
    PRIVATE_HEADERS
        "private/assimp_input.h"
        "private/assimp_output.h"
        "private/dxf_input.h"
        "private/obj_input.h"
        "private/obj_polygonal_output.h"
        "private/obj_triangulated_output.h"
        "private/ply_input.h"
        "private/ply_output.h"
        "private/stl_input.h"
        "private/stl_output.h"
        "private/vtk_input.h"
        "private/vtk_output.h"
        "private/vtp_input.h"
        "private/vtp_output_impl.h"
        "private/vtp_polygonal_output.h"
        "private/vtp_triangulated_output.h"
        "private/vtu_input.h"
        "private/vtu_output.h"
    PRIVATE_DEPENDENCIES
        OpenGeode::basic
        OpenGeode::geometry
        OpenGeode::mesh
        assimp::assimp
        pugixml::pugixml
        zlib::zlibstatic
)
