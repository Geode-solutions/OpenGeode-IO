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

#include <geode/io/model/common.hpp>

PYBIND11_MODULE( opengeode_io_py_model, module )
{
    module.doc() =
        "OpenGeode-IO model formats: registers the readers and writers of BRep "
        "and Section (MSH, SVG, VTM, GID MSH...) in the OpenGeode factories";
    pybind11::class_< geode::OpenGeodeIOModelLibrary >( module,
        "OpenGeodeIOModelLibrary",
        "Initialization of the OpenGeode-IO model library" )
        .def_static( "initialize", &geode::OpenGeodeIOModelLibrary::initialize,
            "Register the BRep and Section readers and writers in the "
            "OpenGeode factories; also initializes OpenGeodeModelLibrary and "
            "OpenGeodeIOMeshLibrary; calling it again has no effect" );
    geode::document_missing_constructors( module );
}