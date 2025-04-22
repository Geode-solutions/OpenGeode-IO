/*
 * Copyright (c) 2019 - 2025 Geode-solutions
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

#include <geode/io/mesh/common.hpp>

#include <geode/mesh/common.hpp>

#include <geode/io/image/common.hpp>

#include <geode/io/mesh/detail/dot_polygonal_output.hpp>
#include <geode/io/mesh/detail/dot_triangulated_output.hpp>
#include <geode/io/mesh/detail/vti_light_regular_grid_input.hpp>
#include <geode/io/mesh/detail/vti_light_regular_grid_output.hpp>
#include <geode/io/mesh/detail/vti_regular_grid_input.hpp>
#include <geode/io/mesh/detail/vti_regular_grid_output.hpp>
#include <geode/io/mesh/detail/vtp_edged_curve_output.hpp>
#include <geode/io/mesh/detail/vtp_input.hpp>
#include <geode/io/mesh/detail/vtp_point_set_output.hpp>
#include <geode/io/mesh/detail/vtp_polygonal_output.hpp>
#include <geode/io/mesh/detail/vtp_triangulated_output.hpp>
#include <geode/io/mesh/detail/vtu_hybrid_input.hpp>
#include <geode/io/mesh/detail/vtu_hybrid_output.hpp>
#include <geode/io/mesh/detail/vtu_polygonal_input.hpp>
#include <geode/io/mesh/detail/vtu_polyhedral_input.hpp>
#include <geode/io/mesh/detail/vtu_polyhedral_output.hpp>
#include <geode/io/mesh/detail/vtu_tetrahedral_input.hpp>
#include <geode/io/mesh/detail/vtu_tetrahedral_output.hpp>
#include <geode/io/mesh/detail/vtu_triangulated_input.hpp>

#include <geode/io/mesh/internal/dxf_input.hpp>
#include <geode/io/mesh/internal/gexf_output.hpp>
#include <geode/io/mesh/internal/obj_input.hpp>
#include <geode/io/mesh/internal/obj_polygonal_output.hpp>
#include <geode/io/mesh/internal/obj_triangulated_output.hpp>
#include <geode/io/mesh/internal/ply_input.hpp>
#include <geode/io/mesh/internal/ply_output.hpp>
#include <geode/io/mesh/internal/smesh_curve_input.hpp>
#include <geode/io/mesh/internal/smesh_triangulated_input.hpp>
#include <geode/io/mesh/internal/stl_input.hpp>
#include <geode/io/mesh/internal/stl_output.hpp>
#include <geode/io/mesh/internal/triangle_output.hpp>

namespace
{
    void register_polygonal_surface_input()
    {
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::internal::DXFInput >(
            geode::internal::DXFInput::extension().data() );
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::internal::PLYInput >(
            geode::internal::PLYInput::extension().data() );
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::internal::OBJInput >(
            geode::internal::OBJInput::extension().data() );
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::detail::VTPInput >(
            geode::detail::VTPInput::extension().data() );
        geode::PolygonalSurfaceInputFactory3D::register_creator<
            geode::detail::VTUPolygonalInput >(
            geode::detail::VTUPolygonalInput::extension().data() );
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
            geode::internal::STLInput >(
            geode::internal::STLInput::extension().data() );
        geode::TriangulatedSurfaceInputFactory3D::register_creator<
            geode::internal::SMESHTriangulatedInput >(
            geode::internal::SMESHTriangulatedInput::extension().data() );
        geode::TriangulatedSurfaceInputFactory3D::register_creator<
            geode::detail::VTUTriangulatedInput >(
            geode::detail::VTUTriangulatedInput::extension().data() );
    }

    void register_polygonal_surface_output()
    {
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::internal::PLYOutput >(
            geode::internal::PLYOutput::extension().data() );
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::internal::OBJPolygonalOutput >(
            geode::internal::OBJPolygonalOutput::extension().data() );
        geode::PolygonalSurfaceOutputFactory2D::register_creator<
            geode::detail::VTPPolygonalOutput< 2 > >(
            geode::detail::VTPPolygonalOutput< 2 >::extension().data() );
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::detail::VTPPolygonalOutput< 3 > >(
            geode::detail::VTPPolygonalOutput< 3 >::extension().data() );
        geode::PolygonalSurfaceOutputFactory3D::register_creator<
            geode::detail::DotPolygonalOutput3D >(
            geode::detail::DotPolygonalOutput3D::extension().data() );
        geode::PolygonalSurfaceOutputFactory2D::register_creator<
            geode::detail::DotPolygonalOutput2D >(
            geode::detail::DotPolygonalOutput2D::extension().data() );
    }

    void register_triangulated_surface_output()
    {
        geode::TriangulatedSurfaceOutputFactory3D::register_creator<
            geode::internal::STLOutput >(
            geode::internal::STLOutput::extension().data() );
        geode::TriangulatedSurfaceOutputFactory3D::register_creator<
            geode::internal::OBJTriangulatedOutput >(
            geode::internal::OBJTriangulatedOutput::extension().data() );
        geode::TriangulatedSurfaceOutputFactory2D::register_creator<
            geode::detail::VTPTriangulatedOutput< 2 > >(
            geode::detail::VTPTriangulatedOutput< 2 >::extension().data() );
        geode::TriangulatedSurfaceOutputFactory3D::register_creator<
            geode::detail::VTPTriangulatedOutput< 3 > >(
            geode::detail::VTPTriangulatedOutput< 3 >::extension().data() );
        geode::TriangulatedSurfaceOutputFactory3D::register_creator<
            geode::detail::DotTriangulatedOutput3D >(
            geode::detail::DotTriangulatedOutput3D::extension().data() );
        geode::TriangulatedSurfaceOutputFactory2D::register_creator<
            geode::detail::DotTriangulatedOutput2D >(
            geode::detail::DotTriangulatedOutput2D::extension().data() );
        geode::TriangulatedSurfaceOutputFactory2D::register_creator<
            geode::internal::TriangleOutput >(
            geode::internal::TriangleOutput::extension().data() );
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
            geode::internal::SMESHCurveInput >(
            geode::internal::SMESHCurveInput::extension().data() );
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

    void register_regular_grid_input()
    {
        geode::RegularGridInputFactory2D::register_creator<
            geode::detail::VTIRegularGridInput< 2 > >(
            geode::detail::VTIRegularGridInput< 2 >::extension().data() );

        geode::RegularGridInputFactory3D::register_creator<
            geode::detail::VTIRegularGridInput< 3 > >(
            geode::detail::VTIRegularGridInput< 3 >::extension().data() );
    }

    void register_light_regular_grid_output()
    {
        geode::LightRegularGridOutputFactory2D::register_creator<
            geode::detail::VTILightRegularGridOutput< 2 > >(
            geode::detail::VTILightRegularGridOutput< 2 >::extension().data() );

        geode::LightRegularGridOutputFactory3D::register_creator<
            geode::detail::VTILightRegularGridOutput< 3 > >(
            geode::detail::VTILightRegularGridOutput< 3 >::extension().data() );
    }

    void register_light_regular_grid_input()
    {
        geode::LightRegularGridInputFactory2D::register_creator<
            geode::detail::VTILightRegularGridInput< 2 > >(
            geode::detail::VTILightRegularGridInput< 2 >::extension().data() );

        geode::LightRegularGridInputFactory3D::register_creator<
            geode::detail::VTILightRegularGridInput< 3 > >(
            geode::detail::VTILightRegularGridInput< 3 >::extension().data() );
    }

    void register_graph_output()
    {
        geode::GraphOutputFactory::register_creator<
            geode::internal::GEXFOutput >(
            geode::internal::GEXFOutput::extension().data() );
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
        register_light_regular_grid_input();
        register_regular_grid_input();
        register_hybrid_solid_input();

        register_point_set_output();
        register_edged_curve_output();
        register_polygonal_surface_output();
        register_triangulated_surface_output();
        register_polyhedral_solid_output();
        register_tetrahedral_solid_output();
        register_light_regular_grid_output();
        register_regular_grid_output();
        register_hybrid_solid_output();
        register_graph_output();
    }
} // namespace geode
