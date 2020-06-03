# -*- coding: utf-8 -*-
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

import os 

import opengeode_py_basic
import opengeode_py_geometry as geom
import opengeode_py_mesh as mesh
import opengeode_py_model as model
import opengeode_io_py_model as model_io

def nb_closed_lines(section):
    nb = 0
    for line in section.lines():
        if section.is_line_closed( line ):
            nb += 1
    return nb

def test_section(section):
    if section.nb_corners() != 31:
        raise ValueError("[Test] Number of corners is not correct")
    if section.nb_lines() != 31:
        raise ValueError("[Test] Number of lines is not correct")
    if nb_closed_lines( section ) != 27:
        raise ValueError("[Test] Number of closed lines is not correct")
    if section.nb_surfaces() != 0:
        raise ValueError("[Test] Number of surfaces is not correct")
    for uv in range(section.nb_unique_vertices()):
        nb_mcv = len(section.mesh_component_vertices(uv))
        if nb_mcv != 1 and nb_mcv != 3:
            raise ValueError("[Test] Wrong number of mesh component vertices for one unique vertex")

if __name__ == '__main__':
    model_io.initialize_model_io()
    test_dir = os.path.dirname(__file__)
    data_dir = os.path.abspath(os.path.join(test_dir, "../../../../tests/data"))

    section = model.Section()
    model.load_section(section, os.path.join(data_dir, "logo.svg"))
    test_section(section)

    model.save_section(section, "logo.og_sctn")
    reloaded_section = model.Section()
    model.load_section(reloaded_section, "logo.og_sctn")
    test_section(reloaded_section)
