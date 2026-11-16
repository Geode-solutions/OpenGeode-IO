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

"""The PVD reader of BRep time series reports the OpenGeode errors."""

import os
import tempfile

import geode_test_env

import opengeode
import opengeode_io

DATA = os.path.abspath(
    os.path.join(os.path.dirname(__file__), "../../../../tests/data")
)


def check(condition, message):
    if not condition:
        raise ValueError("[Test] " + message)


if __name__ == "__main__":
    brep = opengeode.load(
        opengeode.BRep,
        os.path.join(DATA, "spe10", "grid_geos_with_physical_properties.og_brep"),
    )
    pvd = os.path.join(DATA, "spe10", "vtkOutput.pvd")
    check(opengeode.is_brep_time_series_loadable(pvd).value() == 1, "pvd is loadable")
    check(
        "pvd" in opengeode.BRepTimeSeriesInputFactory.list_creators(),
        "The pvd reader should be registered",
    )
    with tempfile.TemporaryDirectory() as directory:
        for name, content in [("garbage.pvd", "garbage\n"), ("other.xyz", "x\n")]:
            filename = os.path.join(directory, name)
            with open(filename, "w") as file:
                file.write(content)
            check(
                opengeode.is_brep_time_series_loadable(filename).value() == 0,
                f"{name} is not loadable",
            )
            try:
                opengeode.load_brep_time_series(brep, filename)
            except opengeode.OpenGeodeDataError as error:
                check(isinstance(error, RuntimeError), "Data errors are RuntimeError")
            else:
                raise ValueError(f"[Test] {name} should not load")
        try:
            opengeode.load_brep_time_series(brep, os.path.join(directory, "none.pvd"))
        except opengeode.OpenGeodeDataError:
            pass
        else:
            raise ValueError("[Test] A missing pvd should not load")
