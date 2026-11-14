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

"""The models of OpenGeode-IO (MSH, SVG, VTM, GID MSH) are read and written
through the generic functions of the opengeode package once opengeode_io is
imported, and the errors are the OpenGeode ones."""

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
    check("msh" in opengeode.input_extensions(opengeode.BRep), "BRep should read msh")
    for extension in ["msh", "gid_msh", "vtm", "og_brep"]:
        check(
            extension in opengeode.output_extensions(opengeode.BRep),
            f"BRep should write {extension}",
        )
    check("svg" in opengeode.input_extensions(opengeode.Section), "Section reads svg")
    for extension in ["vtm", "og_sctn"]:
        check(
            extension in opengeode.output_extensions(opengeode.Section),
            f"Section should write {extension}",
        )
    check(
        "msh" not in opengeode.input_extensions(opengeode.Section),
        "Section should not read msh",
    )


def test_brep():
    cases = [
        ("cube_v41.msh", (8, 12, 6, 1)),
        ("cube_v22.msh", (8, 12, 6, 1)),
        ("cone_v4.msh", (6, 13, 12, 4)),
    ]
    for filename, expected in cases:
        path = os.path.join(DATA, filename)
        check(
            opengeode.is_loadable(opengeode.BRep, path).value() == 1,
            f"{filename} should be loadable",
        )
        brep = opengeode.load(opengeode.BRep, path)
        sizes = (brep.nb_corners(), brep.nb_lines(), brep.nb_surfaces(), brep.nb_blocks())
        check(sizes == expected, f"Wrong components for {filename}: {sizes}")


def test_section():
    path = os.path.join(DATA, "logo.svg")
    check(
        opengeode.is_loadable(opengeode.Section, path).value() == 1,
        "logo.svg should be loadable",
    )
    section = opengeode.load(opengeode.Section, path)
    check(section.nb_corners() == 31 and section.nb_lines() == 31, "Wrong logo.svg")


def test_saving(directory):
    brep = opengeode.load(opengeode.BRep, os.path.join(DATA, "cube_v41.msh"))
    for extension in ["vtm", "og_brep"]:
        filename = os.path.join(directory, "cube." + extension)
        check(opengeode.is_saveable(brep, filename), f"BRep saveable as {extension}")
        saved = opengeode.save(brep, filename)
        check(saved[0] == filename, f"Wrong files saved: {saved}")
        check(os.path.getsize(filename) > 0, f"{filename} is empty")
    # The writers of MSH need a triangulated and tetrahedral model, which is
    # not the case of the polygonal and hybrid meshes read from cube_v41.msh
    for extension in ["msh", "gid_msh", "svg"]:
        check(
            not opengeode.is_saveable(brep, os.path.join(directory, "cube." + extension)),
            f"BRep should not be saveable as {extension}",
        )
    tetrahedral = opengeode.load(opengeode.BRep, os.path.join(DATA, "mss.og_brep"))
    filename = os.path.join(directory, "mss.msh")
    check(opengeode.is_saveable(tetrahedral, filename), "mss should be saveable as msh")
    opengeode.save(tetrahedral, filename)
    reloaded = opengeode.load(opengeode.BRep, filename)
    check(reloaded.nb_blocks() == tetrahedral.nb_blocks(), "msh reload: blocks")
    check(reloaded.nb_surfaces() == tetrahedral.nb_surfaces(), "msh reload: surfaces")
    test_gid(tetrahedral, directory)
    test_vtm(tetrahedral, directory, "mss")

    section = opengeode.load(opengeode.Section, os.path.join(DATA, "logo.svg"))
    filename = os.path.join(directory, "logo.vtm")
    check(opengeode.is_saveable(section, filename), "Section saveable as vtm")
    test_vtm(section, directory, "logo")
    og_filename = os.path.join(directory, "logo.og_sctn")
    check(opengeode.save(section, og_filename) == [og_filename], "og_sctn files")
    reloaded = opengeode.load(opengeode.Section, og_filename)
    check(reloaded.nb_corners() == 31 and reloaded.nb_lines() == 31, "og_sctn reload")
    check(
        reloaded.nb_unique_vertices() == section.nb_unique_vertices(),
        "og_sctn reload: unique vertices",
    )


def test_gid(brep, directory):
    # The GID writer needs a material_number attribute on blocks and surfaces
    gid_filename = os.path.join(directory, "mss.gid_msh")
    try:
        opengeode.save(brep, gid_filename)
    except opengeode.OpenGeodeError as error:
        # The writer error is wrapped by the generic "Cannot save" one
        check("material_number" in str(error), "material_number expected")
    else:
        raise ValueError("[Test] gid_msh needs a material_number attribute")
    for block in brep.blocks():
        block.mesh().polyhedron_attribute_manager().create_attribute_constant_uint(
            "material_number", 1
        )
    for surface in brep.surfaces():
        surface.mesh().polygon_attribute_manager().create_attribute_constant_uint(
            "material_number", 1
        )
    check(opengeode.is_saveable(brep, gid_filename), "mss should be saveable as gid_msh")
    saved = opengeode.save(brep, gid_filename)
    # The file is written next to the requested one, with the extension .gid.msh
    expected = os.path.join(directory, "mss.gid.msh")
    check(
        [same_path(path) for path in saved] == [same_path(expected)], f"gid files: {saved}"
    )
    check(os.path.getsize(expected) > 0, "mss.gid.msh is empty")
    with open(expected) as file:
        content = file.read()
    check("MESH" in content.upper(), "mss.gid.msh is not a GiD mesh")


def same_path(path):
    # The writers build their paths with "/" while os.path.join uses the
    # native separator: compare normalized absolute paths (Windows CI)
    return os.path.normcase(os.path.normpath(os.path.abspath(path)))


def test_vtm(model, directory, name):
    filename = os.path.join(directory, name + ".vtm")
    saved = opengeode.save(model, filename)
    check(same_path(saved[0]) == same_path(filename), f"vtm should come first: {saved[0]}")
    folder = os.path.join(directory, name)
    check(os.path.isdir(folder), f"{folder} should hold the components")
    components = sorted(os.listdir(folder))
    check(
        sorted(same_path(os.path.join(folder, c)) for c in components)
        == sorted(same_path(path) for path in saved[1:]),
        "The companion files returned by save are the ones written",
    )
    kinds = {c.split("_")[0] for c in components}
    expected = {"Corner", "Line"}
    if model.nb_surfaces() > 0:
        expected.add("Surface")
    if isinstance(model, opengeode.BRep):
        expected.add("Block")
    check(kinds == expected, f"Wrong component kinds in the vtm folder: {kinds}")
    nb = model.nb_corners() + model.nb_lines() + model.nb_surfaces()
    if isinstance(model, opengeode.BRep):
        nb += model.nb_blocks()
    check(len(components) == nb, f"{len(components)} files for {nb} components")
    with open(filename) as file:
        content = file.read()
    check(name + "/" in content, "The vtm should reference its folder")


def test_errors(directory):
    garbage = os.path.join(directory, "garbage.msh")
    with open(garbage, "w") as file:
        file.write("not a mesh\n")
    expect_data_error(opengeode.load, opengeode.BRep, garbage)
    check(
        opengeode.is_loadable(opengeode.BRep, garbage).value() == 0,
        "garbage.msh is not loadable",
    )
    expect_data_error(
        opengeode.load, opengeode.BRep, os.path.join(directory, "missing.msh")
    )
    error = expect_data_error(
        opengeode.load, opengeode.Section, os.path.join(DATA, "cube_v41.msh")
    )
    check("Unknown extension" in error.message, "Unknown extension expected")
    expect_data_error(
        opengeode.load, opengeode.Section, os.path.join(directory, "missing.svg")
    )


if __name__ == "__main__":
    test_extensions()
    test_brep()
    test_section()
    with tempfile.TemporaryDirectory() as directory:
        test_saving(directory)
        test_errors(directory)
