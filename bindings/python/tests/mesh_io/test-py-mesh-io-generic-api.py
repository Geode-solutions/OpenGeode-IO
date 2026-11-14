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

"""The meshes of OpenGeode-IO are read and written through the generic
functions of the opengeode package (load, load_all, save, is_loadable,
is_saveable, input_extensions, output_extensions) once opengeode_io is
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
        check(
            isinstance(error, opengeode.OpenGeodeError)
            and isinstance(error, RuntimeError),
            "Data errors must be OpenGeodeError and RuntimeError",
        )
        return error
    raise ValueError("[Test] OpenGeodeDataError expected for " + repr(args))


def check_extensions(cls, inputs, outputs):
    read = opengeode.input_extensions(cls)
    written = opengeode.output_extensions(cls)
    for extension in inputs:
        check(extension in read, f"{cls.__name__} should read {extension}")
    for extension in outputs:
        check(extension in written, f"{cls.__name__} should write {extension}")


def test_extensions():
    check_extensions(
        opengeode.PolygonalSurface3D,
        ["obj", "ply", "vtp", "vtu", "dxf"],
        ["obj", "ply", "vtp"],
    )
    check_extensions(
        opengeode.TriangulatedSurface3D, ["stl", "smesh", "vtu"], ["obj", "stl", "vtp"]
    )
    check_extensions(opengeode.EdgedCurve3D, ["smesh"], ["vtp"])
    check_extensions(opengeode.PolygonalSurface2D, [], ["vtp", "dot"])
    check_extensions(opengeode.TriangulatedSurface2D, [], ["vtp", "dot", "triangle"])
    check_extensions(opengeode.PolygonalSurface3D, [], ["dot"])
    check_extensions(opengeode.TriangulatedSurface3D, [], ["dot"])
    check_extensions(opengeode.EdgedCurve2D, [], ["vtp"])
    check_extensions(opengeode.PointSet2D, [], ["vtp"])
    check_extensions(opengeode.PointSet3D, ["csv", "pnt"], ["vtp"])
    check_extensions(opengeode.TetrahedralSolid3D, ["vtu"], ["vtu"])
    check_extensions(opengeode.PolyhedralSolid3D, ["vtu"], ["vtu"])
    check_extensions(opengeode.HybridSolid3D, ["vtu"], ["vtu"])
    check_extensions(opengeode.RegularGrid2D, ["vti"], ["vti"])
    check_extensions(opengeode.RegularGrid3D, ["vti"], ["vti"])
    check_extensions(opengeode.LightRegularGrid3D, ["vti"], ["vti"])
    # The native formats of OpenGeode stay available
    check_extensions(opengeode.PolygonalSurface3D, ["og_psf3d"], ["og_psf3d"])


def test_is_loadable():
    surface = opengeode.PolygonalSurface3D
    check(
        opengeode.is_loadable(surface, os.path.join(DATA, "TopHat.obj")).value() == 1,
        "TopHat.obj should be loadable",
    )
    check(
        opengeode.is_loadable(surface, os.path.join(DATA, "missing.obj")).value() == 0,
        "A file that does not exist is not loadable",
    )
    check(
        opengeode.is_loadable(surface, os.path.join(DATA, "TopHat.mtl")).value() == 0,
        "An unknown extension is not loadable",
    )
    check(
        opengeode.is_loadable(
            opengeode.TriangulatedSurface3D, os.path.join(DATA, "hat.vtp")
        ).value()
        == 0,
        "A vtp is not loadable as a triangulated surface",
    )


def load(cls, filename):
    return opengeode.load(cls, os.path.join(DATA, filename))


def test_loading():
    cases = [
        (opengeode.PolygonalSurface3D, "TopHat.obj", "nb_polygons", 380, 363),
        (opengeode.PolygonalSurface3D, "Armadillo.ply", "nb_polygons", 345944, 172974),
        (opengeode.PolygonalSurface3D, "face_and_line.dxf", "nb_polygons", 1, 3),
        (opengeode.PolygonalSurface3D, "dfn1_ascii.vtp", "nb_polygons", 10, 187),
        (opengeode.PolygonalSurface3D, "hat.vtp", "nb_polygons", 380, 1480),
        (opengeode.TriangulatedSurface3D, "thumbwheel.stl", "nb_polygons", 1027, 525),
        (opengeode.TriangulatedSurface3D, "triangulated.smesh", "nb_polygons", 186, 111),
        (opengeode.EdgedCurve3D, "curve.smesh", "nb_edges", 1022, 1023),
        (opengeode.PolyhedralSolid3D, "cone.vtu", "nb_polyhedra", 2197, 580),
        (opengeode.PointSet3D, "mount_st_helens_contours.pnt", "nb_vertices", 2603, 2603),
        (opengeode.PointSet3D, "top_granitoid_vertices.csv", "nb_vertices", 103455, 103455),
    ]
    for cls, filename, name, nb_elements, nb_vertices in cases:
        mesh = load(cls, filename)
        check(isinstance(mesh, cls), f"{filename} should be a {cls.__name__}")
        check(
            getattr(mesh, name)() == nb_elements,
            f"Wrong {name} for {filename}: {getattr(mesh, name)()}",
        )
        check(mesh.nb_vertices() == nb_vertices, f"Wrong nb_vertices for {filename}")


def test_load_all():
    grids = opengeode.load_all(
        opengeode.RegularGrid3D, os.path.join(DATA, "two_pieces.vti")
    )
    check([grid.nb_cells() for grid in grids] == [1, 2], "Wrong pieces of vti")
    solids = opengeode.load_all(
        opengeode.TetrahedralSolid3D, os.path.join(DATA, "two_pieces.vtu")
    )
    check([solid.nb_vertices() for solid in solids] == [3, 4], "Wrong pieces of vtu")
    # load needs exactly one object
    expect_data_error(
        opengeode.load, opengeode.RegularGrid3D, os.path.join(DATA, "two_pieces.vti")
    )


def test_saving(directory):
    surface = load(opengeode.PolygonalSurface3D, "TopHat.obj")
    # The extension is case insensitive
    upper = os.path.join(directory, "UPPER.OBJ")
    opengeode.save(surface, upper)
    check(
        opengeode.load(opengeode.PolygonalSurface3D, upper).nb_polygons() == 380,
        "UPPER.OBJ reload: polygons",
    )
    for extension in ["obj", "ply", "vtp"]:
        filename = os.path.join(directory, "TopHat." + extension)
        check(opengeode.is_saveable(surface, filename), f"{extension} should be saveable")
        saved = opengeode.save(surface, filename)
        check(saved[0] == filename, f"save should return the files, got {saved}")
        check(os.path.getsize(filename) > 0, f"{filename} is empty")
    for extension in ["obj", "ply"]:
        reloaded = opengeode.load(
            opengeode.PolygonalSurface3D,
            os.path.join(directory, "TopHat." + extension),
        )
        check(reloaded.nb_vertices() == 363, f"{extension} reload: vertices")
        check(reloaded.nb_polygons() == 380, f"{extension} reload: polygons")
    reloaded = opengeode.load(
        opengeode.PolygonalSurface3D, os.path.join(directory, "TopHat.vtp")
    )
    check(reloaded.nb_polygons() == 380, "vtp reload: polygons")
    check(
        not opengeode.is_saveable(surface, os.path.join(directory, "TopHat.stl")),
        "A polygonal surface is not saveable as stl",
    )

    triangulated = load(opengeode.TriangulatedSurface3D, "thumbwheel.stl")
    for extension in ["stl", "obj", "vtp"]:
        filename = os.path.join(directory, "thumbwheel." + extension)
        opengeode.save(triangulated, filename)
        check(os.path.getsize(filename) > 0, f"{filename} is empty")
    reloaded = opengeode.load(
        opengeode.TriangulatedSurface3D, os.path.join(directory, "thumbwheel.stl")
    )
    check(reloaded.nb_polygons() == 1027, "stl reload: polygons")

    solid = load(opengeode.TetrahedralSolid3D, "cone.vtu")
    filename = os.path.join(directory, "cone.vtu")
    opengeode.save(solid, filename)
    reloaded = opengeode.load(opengeode.TetrahedralSolid3D, filename)
    check(reloaded.nb_polyhedra() == solid.nb_polyhedra(), "vtu reload: polyhedra")

    points = load(opengeode.PointSet3D, "mount_st_helens_contours.pnt")
    filename = os.path.join(directory, "points.vtp")
    opengeode.save(points, filename)
    check(os.path.getsize(filename) > 0, "points.vtp is empty")

    grid = opengeode.load_all(
        opengeode.RegularGrid3D, os.path.join(DATA, "two_pieces.vti")
    )[1]
    filename = os.path.join(directory, "grid.vti")
    opengeode.save(grid, filename)
    reloaded = opengeode.load(opengeode.RegularGrid3D, filename)
    check(reloaded.nb_cells() == 2, "vti reload: cells")


def test_saving_2D(directory):
    triangles = opengeode.TriangulatedSurface2D.create()
    builder = opengeode.TriangulatedSurfaceBuilder2D.create(triangles)
    for point in ([0, 0], [1, 0], [0, 1], [1, 1]):
        builder.create_point(opengeode.Point2D(point))
    builder.create_triangle([0, 1, 2])
    builder.create_triangle([1, 3, 2])
    polygons = opengeode.PolygonalSurface2D.create()
    builder = opengeode.PolygonalSurfaceBuilder2D.create(polygons)
    for point in ([0, 0], [1, 0], [1, 1], [0, 1]):
        builder.create_point(opengeode.Point2D(point))
    builder.create_polygon([0, 1, 2, 3])
    for surface, name in ((triangles, "triangles"), (polygons, "polygons")):
        for extension in ["vtp", "dot"]:
            filename = os.path.join(directory, f"{name}.{extension}")
            check(opengeode.is_saveable(surface, filename), f"{name}: {extension}")
            check(opengeode.save(surface, filename) == [filename], f"{name} files")
            check(os.path.getsize(filename) > 0, f"{filename} is empty")
        # There are no 2D readers: the 2D vtp is read back as a 3D surface
        reloaded = opengeode.load(
            opengeode.PolygonalSurface3D, os.path.join(directory, name + ".vtp")
        )
        check(reloaded.nb_vertices() == 4, f"{name}: vtp reload vertices")
        check(reloaded.nb_polygons() == surface.nb_polygons(), f"{name}: vtp reload")
        check(reloaded.point(2).value(2) == 0, f"{name}: vtp reload is flat")
    filename = os.path.join(directory, "triangles.triangle")
    check(opengeode.is_saveable(triangles, filename), "triangles: triangle")
    saved = opengeode.save(triangles, filename)
    expected = [os.path.join(directory, "triangles." + e) for e in ("node", "ele", "neigh")]
    check(saved == expected, f"Wrong Triangle files: {saved}")
    for path in expected:
        check(os.path.getsize(path) > 0, f"{path} is empty")
    check(
        not opengeode.is_saveable(polygons, os.path.join(directory, "p.triangle")),
        "Polygonal surfaces are not saveable as Triangle files",
    )


def test_errors(directory):
    garbage = os.path.join(directory, "garbage.obj")
    with open(garbage, "w") as file:
        file.write("garbage\n")
    error = expect_data_error(opengeode.load, opengeode.PolygonalSurface3D, garbage)
    check("garbage.obj" in error.message, "The error should name the file")
    check(
        opengeode.is_loadable(opengeode.PolygonalSurface3D, garbage).value() == 0,
        "garbage.obj is not loadable",
    )
    expect_data_error(
        opengeode.load,
        opengeode.PolygonalSurface3D,
        os.path.join(directory, "missing.obj"),
    )
    unknown = os.path.join(directory, "surface.xyz")
    with open(unknown, "w") as file:
        file.write("x\n")
    error = expect_data_error(opengeode.load, opengeode.PolygonalSurface3D, unknown)
    check("Unknown extension" in error.message, "Unknown extension expected")
    try:
        opengeode.save(
            load(opengeode.PolygonalSurface3D, "TopHat.obj"),
            os.path.join(directory, "surface.xyz"),
        )
    except opengeode.OpenGeodeError:
        pass
    else:
        raise ValueError("[Test] Saving as an unknown extension should fail")


if __name__ == "__main__":
    test_extensions()
    test_is_loadable()
    test_loading()
    test_load_all()
    with tempfile.TemporaryDirectory() as directory:
        test_saving(directory)
        test_saving_2D(directory)
        test_errors(directory)
