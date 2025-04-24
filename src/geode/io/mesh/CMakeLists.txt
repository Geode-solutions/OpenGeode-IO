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
    NAME mesh
    FOLDER "geode/io/mesh"
    SOURCES
        "assimp_input.cpp"
        "common.cpp"
        "dot_polygonal_output.cpp"
        "dot_triangulated_output.cpp"
        "dxf_input.cpp"
        "gexf_output.cpp"
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
        "vtu_polygonal_input.cpp"
        "vtu_polyhedral_input.cpp"
        "vtu_polyhedral_output.cpp"
        "vtu_tetrahedral_input.cpp"
        "vtu_tetrahedral_output.cpp"
        "vtu_triangulated_input.cpp"
    PUBLIC_HEADERS
        "common.hpp"
    ADVANCED_HEADERS
        "detail/dot_polygonal_output.hpp"
        "detail/dot_surface_output_impl.hpp"
        "detail/dot_triangulated_output.hpp"
        "detail/vtk_input.hpp"
        "detail/vtk_mesh_input.hpp"
        "detail/vtk_mesh_output.hpp"
        "detail/vti_grid_input.hpp"
        "detail/vti_grid_output.hpp"
        "detail/vti_light_regular_grid_input.hpp"
        "detail/vti_light_regular_grid_output.hpp"
        "detail/vti_regular_grid_input.hpp"
        "detail/vti_regular_grid_output.hpp"
        "detail/vtp_edged_curve_output.hpp"
        "detail/vtp_input.hpp"
        "detail/vtp_point_set_output.hpp"
        "detail/vtp_polygonal_output.hpp"
        "detail/vtp_surface_output_impl.hpp"
        "detail/vtp_triangulated_output.hpp"
        "detail/vtu_hybrid_output.hpp"
        "detail/vtu_input_impl.hpp"
        "detail/vtu_polygonal_input.hpp"
        "detail/vtu_surface_input.hpp"
        "detail/vtu_solid_input.hpp"
        "detail/vtu_triangulated_input.hpp"
        "detail/vtu_output_impl.hpp"
        "detail/vtu_hybrid_input.hpp"
        "detail/vtu_polyhedral_input.hpp"
        "detail/vtu_polyhedral_output.hpp"
        "detail/vtu_tetrahedral_input.hpp"
        "detail/vtu_tetrahedral_output.hpp"
    INTERNAL_HEADERS
        "internal/assimp_input.hpp"
        "internal/assimp_output.hpp"
        "internal/dxf_input.hpp"
        "internal/gexf_output.hpp"
        "internal/obj_input.hpp"
        "internal/obj_polygonal_output.hpp"
        "internal/obj_triangulated_output.hpp"
        "internal/ply_input.hpp"
        "internal/ply_output.hpp"
        "internal/smesh_curve_input.hpp"
        "internal/smesh_input.hpp"
        "internal/smesh_triangulated_input.hpp"
        "internal/stl_input.hpp"
        "internal/stl_output.hpp"
        "internal/triangle_output.hpp"
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
