/*
 * Copyright (c) 2019 - 2026 Geode-solutions
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <pybind11/pybind11.h>

#include <geode/bindings/python/common.hpp>

#include <geode/io/mesh/common.hpp>

PYBIND11_MODULE( opengeode_io_py_mesh, module )
{
    module.doc() =
        "OpenGeode-IO mesh formats: registers the readers and writers of "
        "points, curves, surfaces (2D and 3D), solids and grids (OBJ, PLY, "
        "STL, VTP, VTU, VTI, SMESH, DXF, CSV, PNT, DOT, Triangle...) in the "
        "OpenGeode factories";
    pybind11::class_< geode::OpenGeodeIOMeshLibrary >( module,
        "OpenGeodeIOMeshLibrary",
        "Initialization of the OpenGeode-IO mesh library" )
        .def_static( "initialize", &geode::OpenGeodeIOMeshLibrary::initialize,
            "Register the mesh readers and writers in the OpenGeode factories; "
            "also initializes OpenGeodeMeshLibrary and "
            "OpenGeodeIOImageLibrary; "
            "calling it again has no effect" );
    geode::document_missing_constructors( module );
}