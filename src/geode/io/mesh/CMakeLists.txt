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
        "smesh_curve_input.cpp"
        "smesh_triangulated_input.cpp"
        "stl_input.cpp"
        "stl_output.cpp"
        "triangle_output.cpp"
        "vti_light_regular_grid_input.cpp"
        "vti_light_regular_grid_output.cpp"
        "vti_regular_grid_input.cpp"
        "vti_regular_grid_output.cpp"
        "vtp_edged_curve_output.cpp"
        "vtp_input.cpp"
        "vtp_point_set_output.cpp"
        "vtp_polygonal_output.cpp"
        "vtp_triangulated_output.cpp"
        "vtu_hybrid_input.cpp"
        "vtu_hybrid_output.cpp"
        "vtu_polyhedral_input.cpp"
        "vtu_polyhedral_output.cpp"
        "vtu_tetrahedral_input.cpp"
        "vtu_tetrahedral_output.cpp"
    PUBLIC_HEADERS
        "common.h"
    ADVANCED_HEADERS
        "detail/vtk_input.h"
        "detail/vtk_mesh_input.h"
        "detail/vtk_mesh_output.h"
        "detail/vti_grid_input.h"
        "detail/vti_grid_output.h"
        "detail/vti_light_regular_grid_input.h"
        "detail/vti_light_regular_grid_output.h"
        "detail/vti_regular_grid_input.h"
        "detail/vti_regular_grid_output.h"
        "detail/vtp_edged_curve_output.h"
        "detail/vtp_input.h"
        "detail/vtp_point_set_output.h"
        "detail/vtp_polygonal_output.h"
        "detail/vtp_surface_output_impl.h"
        "detail/vtp_triangulated_output.h"
        "detail/vtu_hybrid_output.h"
        "detail/vtu_input_impl.h"
        "detail/vtu_output_impl.h"
        "detail/vtu_hybrid_input.h"
        "detail/vtu_polyhedral_input.h"
        "detail/vtu_polyhedral_output.h"
        "detail/vtu_tetrahedral_input.h"
        "detail/vtu_tetrahedral_output.h"
    INTERNAL_HEADERS
        "internal/assimp_input.h"
        "internal/assimp_output.h"
        "internal/dxf_input.h"
        "internal/obj_input.h"
        "internal/obj_polygonal_output.h"
        "internal/obj_triangulated_output.h"
        "internal/ply_input.h"
        "internal/ply_output.h"
        "internal/smesh_curve_input.h"
        "internal/smesh_input.h"
        "internal/smesh_triangulated_input.h"
        "internal/stl_input.h"
        "internal/stl_output.h"
        "internal/triangle_output.h"
    PUBLIC_DEPENDENCIES
        OpenGeode::image
    PRIVATE_DEPENDENCIES
        OpenGeode::basic
        OpenGeode::geometry
        OpenGeode::mesh
        assimp::assimp
        pugixml::pugixml
        ZLIB::ZLIB
        Async++
        ${PROJECT_NAME}::image
)
