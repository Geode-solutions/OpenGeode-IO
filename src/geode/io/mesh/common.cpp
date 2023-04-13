/*
 * Copyright (c) 2019 - 2023 Geode-solutions
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

#include <geode/io/mesh/common.h>

#include <geode/mesh/common.h>

#include <geode/io/image/common.h>

#include <geode/io/mesh/private/dxf_input.h>
#include <geode/io/mesh/private/obj_input.h>
#include <geode/io/mesh/private/obj_polygonal_output.h>
#include <geode/io/mesh/private/obj_triangulated_output.h>
#include <geode/io/mesh/private/ply_input.h>
#include <geode/io/mesh/private/ply_output.h>
#include <geode/io/mesh/private/smesh_curve_input.h>
#include <geode/io/mesh/private/smesh_triangulated_input.h>
#include <geode/io/mesh/private/stl_input.h>
#include <geode/io/mesh/private/stl_output.h>
#include <geode/io/mesh/private/triangle_output.h>
#include <geode/io/mesh/private/vti_regular_grid_output.h>
#include <geode/io/mesh/private/vtp_edged_curve_output.h>
#include <geode/io/mesh/private/vtp_input.h>
#include <geode/io/mesh/private/vtp_point_set_output.h>
#include <geode/io/mesh/private/vtp_polygonal_output.h>
#include <geode/io/mesh/private/vtp_triangulated_output.h>
#include <geode/io/mesh/private/vtu_hybrid_input.h>
#include <geode/io/mesh/private/vtu_hybrid_output.h>
#include <geode/io/mesh/private/vtu_polyhedral_input.h>
#include <geode/io/mesh/private/vtu_polyhedral_output.h>
#include <geode/io/mesh/private/vtu_tetrahedral_input.h>
#include <geode/io/mesh/private/vtu_tetrahedral_output.h>

namespace
{
    void register_polygonal_surface_input()
    {
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::detail::DXFInput >(
            geode::detail::DXFInput::extension().data() );
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::detail::PLYInput >(
            geode::detail::PLYInput::extension().data() );
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::detail::OBJInput >(
            geode::detail::OBJInput::extension().data() );
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::detail::VTPInput >(
            geode::detail::VTPInput::extension().data() );
    }

    void register_polyhedral_solid_input()
    {
        geode::PolyhedralSolidInputFactory3D::register_creator<
            geode::detail::VTUPolyhedralInput >(
            geode::detail::VTUPolyhedralInput::extension().data() );
    }

    void register_tetrahedral_solid_input()
    {
        geode::TetrahedralSolidInputFactory3D::register_creator<
            geode::detail::VTUTetrahedralInput >(
            geode::detail::VTUTetrahedralInput::extension().data() );
    }

    void register_hybrid_solid_input()
    {
        geode::HybridSolidInputFactory3D::register_creator<
            geode::detail::VTUHybridInput >(
            geode::detail::VTUHybridInput::extension().data() );
    }

    void register_triangulated_surface_input()
    {
        geode::TriangulatedSurfaceInputFactory3D::register_creator<
            geode::detail::STLInput >(
            geode::detail::STLInput::extension().data() );
        geode::TriangulatedSurfaceInputFactory3D::register_creator<
            geode::detail::SMESHTriangulatedInput >(
            geode::detail::SMESHTriangulatedInput::extension().data() );
    }

    void register_polygonal_surface_output()
    {
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::detail::PLYOutput >(
            geode::detail::PLYOutput::extension().data() );
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::detail::OBJPolygonalOutput >(
            geode::detail::OBJPolygonalOutput::extension().data() );
        geode::PolygonalSurfaceOutputFactory2D::register_creator<
            geode::detail::VTPPolygonalOutput< 2 > >(
            geode::detail::VTPPolygonalOutput< 2 >::extension().data() );
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::detail::VTPPolygonalOutput< 3 > >(
            geode::detail::VTPPolygonalOutput< 3 >::extension().data() );
    }

    void register_triangulated_surface_output()
    {
        geode::TriangulatedSurfaceOutputFactory3D::register_creator<
            geode::detail::STLOutput >(
            geode::detail::STLOutput::extension().data() );
        geode::TriangulatedSurfaceOutputFactory3D::register_creator<
            geode::detail::OBJTriangulatedOutput >(
            geode::detail::OBJTriangulatedOutput::extension().data() );
        geode::TriangulatedSurfaceOutputFactory2D::register_creator<
            geode::detail::VTPTriangulatedOutput< 2 > >(
            geode::detail::VTPTriangulatedOutput< 2 >::extension().data() );
        geode::TriangulatedSurfaceOutputFactory3D::register_creator<
            geode::detail::VTPTriangulatedOutput< 3 > >(
            geode::detail::VTPTriangulatedOutput< 3 >::extension().data() );

        geode::TriangulatedSurfaceOutputFactory2D::register_creator<
            geode::detail::TriangleOutput >(
            geode::detail::TriangleOutput::extension().data() );
    }

    void register_polyhedral_solid_output()
    {
        geode::PolyhedralSolidOutputFactory3D::register_creator<
            geode::detail::VTUPolyhedralOutput >(
            geode::detail::VTUPolyhedralOutput::extension().data() );
    }

    void register_tetrahedral_solid_output()
    {
        geode::TetrahedralSolidOutputFactory3D::register_creator<
            geode::detail::VTUTetrahedralOutput >(
            geode::detail::VTUTetrahedralOutput::extension().data() );
    }

    void register_hybrid_solid_output()
    {
        geode::HybridSolidOutputFactory3D::register_creator<
            geode::detail::VTUHybridOutput >(
            geode::detail::VTUHybridOutput::extension().data() );
    }

    void register_point_set_output()
    {
        geode::PointSetOutputFactory2D::register_creator<
            geode::detail::VTPPointSetOutput< 2 > >(
            geode::detail::VTPPointSetOutput< 2 >::extension().data() );
        geode::PointSetOutputFactory3D::register_creator<
            geode::detail::VTPPointSetOutput< 3 > >(
            geode::detail::VTPPointSetOutput< 3 >::extension().data() );
    }

    void register_edged_curve_output()
    {
        geode::EdgedCurveOutputFactory2D::register_creator<
            geode::detail::VTPEdgedCurveOutput< 2 > >(
            geode::detail::VTPEdgedCurveOutput< 2 >::extension().data() );
        geode::EdgedCurveOutputFactory3D::register_creator<
            geode::detail::VTPEdgedCurveOutput< 3 > >(
            geode::detail::VTPEdgedCurveOutput< 3 >::extension().data() );
    }

    void register_edged_curve_input()
    {
        geode::EdgedCurveInputFactory3D::register_creator<
            geode::detail::SMESHCurveInput >(
            geode::detail::SMESHCurveInput::extension().data() );
    }

    void register_regular_grid_output()
    {
        geode::RegularGridOutputFactory2D::register_creator<
            geode::detail::VTIRegularGridOutput< 2 > >(
            geode::detail::VTIRegularGridOutput< 2 >::extension().data() );

        geode::RegularGridOutputFactory3D::register_creator<
            geode::detail::VTIRegularGridOutput< 3 > >(
            geode::detail::VTIRegularGridOutput< 3 >::extension().data() );
    }
} // namespace

namespace geode
{
    OPENGEODE_LIBRARY_IMPLEMENTATION( IOMesh )
    {
        OpenGeodeMeshLibrary::initialize();
        IOImageLibrary::initialize();

        register_edged_curve_input();
        register_polygonal_surface_input();
        register_triangulated_surface_input();
        register_polyhedral_solid_input();
        register_tetrahedral_solid_input();
        register_hybrid_solid_input();

        register_point_set_output();
        register_edged_curve_output();
        register_polygonal_surface_output();
        register_triangulated_surface_output();
        register_polyhedral_solid_output();
        register_tetrahedral_solid_output();
        register_regular_grid_output();
        register_hybrid_solid_output();
    }
} // namespace geode
