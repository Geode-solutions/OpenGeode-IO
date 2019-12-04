/*
 * Copyright (c) 2019 Geode-solutions
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

#include <geode/mesh/detail/common.h>

#include <geode/mesh/detail/obj_input.h>
#include <geode/mesh/detail/obj_output.h>
#include <geode/mesh/detail/ply_input.h>
#include <geode/mesh/detail/ply_output.h>
#include <geode/mesh/detail/stl_input.h>
#include <geode/mesh/detail/stl_output.h>

namespace
{
    void register_polygonal_surface_input()
    {
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::PLYInput >( geode::PLYInput::extension() );
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::OBJInput >( geode::OBJInput::extension() );
    }

    void register_triangulated_surface_input()
    {
        geode::TriangulatedSurfaceInputFactory3D::register_creator<
            geode::STLInput >( geode::STLInput::extension() );
    }

    void register_polygonal_surface_output()
    {
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::PLYOutput >( geode::PLYOutput::extension() );
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::OBJOutput >( geode::OBJOutput::extension() );
    }

    void register_triangulated_surface_output()
    {
        geode::TriangulatedSurfaceOutputFactory3D::register_creator<
            geode::STLOutput >( geode::STLOutput::extension() );
    }

    OPENGEODE_LIBRARY_INITIALIZE( OpenGeode_IO_mesh )
    {
        register_polygonal_surface_input();
        register_triangulated_surface_input();

        register_polygonal_surface_output();
        register_triangulated_surface_output();
    }
} // namespace

namespace geode
{
    void initialize_mesh_io() {}
} // namespace geode