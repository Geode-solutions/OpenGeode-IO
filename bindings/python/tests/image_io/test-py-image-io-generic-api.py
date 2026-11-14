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

"""The raster images of OpenGeode-IO (BMP, JPG, PNG, TIFF readers, VTI writer)
are read and written through the generic functions of the opengeode package
once opengeode_io is imported, and the errors are the OpenGeode ones."""

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


def expect_data_error(function, *args):
    try:
        function(*args)
    except opengeode.OpenGeodeDataError as error:
        check(isinstance(error, RuntimeError), "Data errors are RuntimeError")
        return error
    raise ValueError("[Test] OpenGeodeDataError expected for " + repr(args))


def test_extensions():
    read = opengeode.input_extensions(opengeode.RasterImage2D)
    for extension in ["bmp", "jpg", "png", "tif", "tiff"]:
        check(extension in read, f"RasterImage2D should read {extension}")
    written = opengeode.output_extensions(opengeode.RasterImage2D)
    check("vti" in written, "RasterImage2D should write vti")
    check("png" not in written, "RasterImage2D should not write png")


def test_loading():
    cases = [
        ("grid_image.png", 405576),
        ("grid_image_from_gimp.jpg", 405576),
        ("TopHat.bmp", 262144),
        ("cea.tiff", 264710),
        ("image_rgb.png", 12),
    ]
    for filename, nb_cells in cases:
        path = os.path.join(DATA, filename)
        check(
            opengeode.is_loadable(opengeode.RasterImage2D, path).value() == 1,
            f"{filename} should be loadable",
        )
        image = opengeode.load(opengeode.RasterImage2D, path)
        check(isinstance(image, opengeode.RasterImage2D), "RasterImage2D expected")
        check(image.nb_cells() == nb_cells, f"Wrong nb_cells for {filename}")


def test_saving(directory):
    image = opengeode.load(
        opengeode.RasterImage2D, os.path.join(DATA, "image_rgb.png")
    )
    filename = os.path.join(directory, "image.vti")
    check(opengeode.is_saveable(image, filename), "RasterImage2D saveable as vti")
    opengeode.save(image, filename)
    check(os.path.getsize(filename) > 0, "image.vti is empty")
    check(
        not opengeode.is_saveable(image, os.path.join(directory, "image.png")),
        "RasterImage2D is not saveable as png",
    )


def test_errors(directory):
    expect_data_error(
        opengeode.load,
        opengeode.RasterImage2D,
        os.path.join(DATA, "image_garbage.png"),
    )
    expect_data_error(
        opengeode.load,
        opengeode.RasterImage2D,
        os.path.join(directory, "missing.png"),
    )
    error = expect_data_error(
        opengeode.load, opengeode.RasterImage2D, os.path.join(DATA, "TopHat.obj")
    )
    check("Unknown extension" in error.message, "Unknown extension expected")


if __name__ == "__main__":
    test_extensions()
    test_loading()
    with tempfile.TemporaryDirectory() as directory:
        test_saving(directory)
        test_errors(directory)
