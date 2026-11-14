# -*- coding: utf-8 -*-
# Copyright (c) 2019 - 2026 Geode-solutions
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
import tempfile

import geode_test_env

import opengeode
import opengeode_io_py_mesh as mesh_io


def check(surface, nb_vertices, nb_polygons):
    if surface.nb_vertices() != nb_vertices:
        raise ValueError(
            "[Test] Number of vertices in the loaded Surface is not correct"
        )
    if surface.nb_polygons() != nb_polygons:
        raise ValueError(
            "[Test] Number of polygons in the loaded Surface is not correct"
        )


def run_test(output_dir, filename, nb_vertices, nb_polygons):
    test_dir = os.path.dirname(__file__)
    data_dir = os.path.abspath(os.path.join(test_dir, "../../../../tests/data"))
    surface_filename = os.path.join(data_dir, filename + ".vtp")
    surface = opengeode.load_one(
        opengeode.load_polygonal_surface3D(surface_filename), surface_filename
    )
    check(surface, nb_vertices, nb_polygons)
    reloaded_filename = os.path.join(output_dir, filename + ".og_psf3d")
    opengeode.save_polygonal_surface3D(surface, reloaded_filename)
    reloaded_surface = opengeode.load_one(
        opengeode.load_polygonal_surface3D(reloaded_filename), reloaded_filename
    )
    check(reloaded_surface, nb_vertices, nb_polygons)


if __name__ == "__main__":
    mesh_io.OpenGeodeIOMeshLibrary.initialize()
    with tempfile.TemporaryDirectory() as output_dir:
        run_test(output_dir, "dfn1_ascii", 187, 10)
        run_test(output_dir, "dfn2_mesh_compressed", 33413, 58820)
