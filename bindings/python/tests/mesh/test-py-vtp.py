# -*- coding: utf-8 -*-
# Copyright (c) 2019 - 2023 Geode-solutions
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

import os
import sys
import platform
if sys.version_info >= (3, 8, 0) and platform.system() == "Windows":
    for path in [x.strip() for x in os.environ['PATH'].split(';') if x]:
        os.add_dll_directory(path)

import opengeode
import opengeode_io_py_mesh as mesh_io


def check(surface, nb_vertices, nb_polygons):
    if surface.nb_vertices() != nb_vertices:
        raise ValueError(
            "[Test] Number of vertices in the loaded Surface is not correct")
    if surface.nb_polygons() != nb_polygons:
        raise ValueError(
            "[Test] Number of polygons in the loaded Surface is not correct")


def run_test(filename, nb_vertices, nb_polygons):
    test_dir = os.path.dirname(__file__)
    data_dir = os.path.abspath(os.path.join(
        test_dir, "../../../../tests/data"))
    surface = opengeode.load_polygonal_surface3D(
        os.path.join(data_dir, filename + ".vtp"))
    check(surface, nb_vertices, nb_polygons)
    opengeode.save_polygonal_surface3D(
        surface, os.path.join(filename + ".og_psf3d"))
    reloaded_surface = opengeode.load_polygonal_surface3D(
        os.path.join(filename + ".og_psf3d"))
    check(surface, nb_vertices, nb_polygons)


if __name__ == '__main__':
    mesh_io.IOMeshLibrary.initialize()
    run_test("dfn1_ascii", 187, 10)
    run_test("dfn2_mesh_compressed", 33413, 58820)
