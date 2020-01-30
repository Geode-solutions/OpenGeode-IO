/*
 * Copyright (c) 2019 - 2020 Geode-solutions
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

#include <geode/io/mesh/detail/common.h>

#include <geode/io/mesh/private/obj_input.h>
#include <geode/io/mesh/private/obj_output.h>
#include <geode/io/mesh/private/ply_input.h>
#include <geode/io/mesh/private/ply_output.h>
#include <geode/io/mesh/private/stl_input.h>
#include <geode/io/mesh/private/stl_output.h>

namespace
{
    void register_polygonal_surface_input()
    {
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::detail::PLYInput >(
            geode::detail::PLYInput::extension().data() );
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::detail::OBJInput >(
            geode::detail::OBJInput::extension().data() );
    }

    void register_triangulated_surface_input()
    {
        geode::TriangulatedSurfaceInputFactory3D::register_creator<
            geode::detail::STLInput >(
            geode::detail::STLInput::extension().data() );
    }

    void register_polygonal_surface_output()
    {
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::detail::PLYOutput >(
            geode::detail::PLYOutput::extension().data() );
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::detail::OBJOutput >(
            geode::detail::OBJOutput::extension().data() );
    }

    void register_triangulated_surface_output()
    {
        geode::TriangulatedSurfaceOutputFactory3D::register_creator<
            geode::detail::STLOutput >(
            geode::detail::STLOutput::extension().data() );
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
    namespace detail
    {
        void initialize_mesh_io()
        {
        }
    } // namespace detail
} // namespace geode