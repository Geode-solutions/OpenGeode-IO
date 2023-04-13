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
import opengeode_io_py_model as model_io


def nb_closed_lines(section):
    nb = 0
    for line in section.lines():
        if section.is_line_closed(line):
            nb += 1
    return nb


def test_section(section):
    if section.nb_corners() != 31:
        raise ValueError("[Test] Number of corners is not correct")
    if section.nb_lines() != 31:
        raise ValueError("[Test] Number of lines is not correct")
    if nb_closed_lines(section) != 27:
        raise ValueError("[Test] Number of closed lines is not correct")
    if section.nb_surfaces() != 0:
        raise ValueError("[Test] Number of surfaces is not correct")
    for uv in range(section.nb_unique_vertices()):
        nb_cmv = len(section.component_mesh_vertices(uv))
        if nb_cmv != 1 and nb_cmv != 3:
            raise ValueError(
                "[Test] Wrong number of mesh component vertices for one unique vertex")


if __name__ == '__main__':
    model_io.IOModelLibrary.initialize()
    test_dir = os.path.dirname(__file__)
    data_dir = os.path.abspath(os.path.join(
        test_dir, "../../../../tests/data"))

    section = opengeode.load_section(os.path.join(data_dir, "logo.svg"))
    test_section(section)

    opengeode.save_section(section, "logo.og_sctn")
    reloaded_section = opengeode.load_section("logo.og_sctn")
    test_section(reloaded_section)
