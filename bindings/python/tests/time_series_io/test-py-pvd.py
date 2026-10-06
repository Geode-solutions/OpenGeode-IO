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
import sys
import platform

if sys.version_info >= (3, 8, 0) and platform.system() == "Windows":
    for path in [x.strip() for x in os.environ["PATH"].split(";") if x]:
        os.add_dll_directory(path)

import opengeode
import opengeode_io_py_time_series as time_series_io

TIMES = [0.0, 1e6, 2e6]


def test_block(block):
    manager = block.mesh().polyhedron_attribute_manager()
    for name in ["pressure", "temperature"]:
        series = opengeode.AttributeTimeSeriesDouble(manager, name)
        if series.nb_time_steps() != len(TIMES):
            raise ValueError("[Test] Wrong number of time steps for " + name)
        for step, time in enumerate(TIMES):
            if series.time(step) != time:
                raise ValueError("[Test] Wrong time for " + name)


if __name__ == "__main__":
    time_series_io.OpenGeodeIOTimeSeriesLibrary.initialize()
    test_dir = os.path.dirname(__file__)
    data_dir = os.path.abspath(
        os.path.join(test_dir, "../../../../tests/data/spe10")
    )
    brep = opengeode.load_brep(
        os.path.join(data_dir, "grid_geos_with_physical_properties.og_brep")
    )
    pvd = os.path.join(data_dir, "vtkOutput.pvd")
    if opengeode.is_brep_time_series_loadable(pvd).value() != 1:
        raise ValueError("[Test] Collection should be loadable")

    opengeode.load_brep_time_series(brep, pvd)
    for block in brep.blocks():
        test_block(block)
