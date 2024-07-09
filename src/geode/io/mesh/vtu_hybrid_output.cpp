/*
 * Copyright (c) 2019 - 2024 Geode-solutions
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

#include <geode/io/mesh/detail/vtu_hybrid_output.h>

#include <string>

#include <geode/mesh/core/hybrid_solid.h>

#include <geode/io/mesh/detail/vtu_output_impl.h>
namespace
{
    class VTUHybridOutputImpl
        : public geode::detail::VTUOutputImpl< geode::HybridSolid >
    {
    public:
        VTUHybridOutputImpl(
            std::string_view filename, const geode::HybridSolid3D& solid )
            : geode::detail::VTUOutputImpl< geode::HybridSolid >{ filename,
                  solid }
        {
        }

    private:
        void write_cell( geode::index_t p,
            std::string& cell_types,
            std::string& /*unused*/,
            std::string& /*unused*/,
            geode::index_t& /*unused*/ ) const override
        {
            const auto nb_vertices = this->mesh().nb_polyhedron_vertices( p );
            const auto vtk_type =
                geode::detail::VTK_NB_VERTICES_TO_CELL_TYPE[nb_vertices];
            OPENGEODE_EXCEPTION( vtk_type != 0,
                "[VTUHybridOutputImpl::write_vtk_cell] Polyhedron with ",
                nb_vertices, " vertices not supported" );
            absl::StrAppend( &cell_types, vtk_type, " " );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        std::vector< std::string > VTUHybridOutput::write(
            const HybridSolid3D& solid ) const
        {
            VTUHybridOutputImpl impl{ filename(), solid };
            impl.write_file();
            return { to_string( filename() ) };
        }
    } // namespace detail
} // namespace geode
