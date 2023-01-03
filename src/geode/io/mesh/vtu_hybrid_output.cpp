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

#include <geode/io/mesh/private/vtu_hybrid_output.h>

#include <geode/mesh/core/hybrid_solid.h>

#include <geode/io/mesh/private/vtu_output_impl.h>
namespace
{
    static constexpr auto TETRAHEDRON = 10u;
    static constexpr auto HEXAHEDRON = 12u;
    static constexpr auto PRISM = 13u;
    static constexpr auto PYRAMID = 14u;
    static constexpr std::array< geode::index_t, 9 > VTK_CELL_TYPE{ 0, 0, 0, 0,
        TETRAHEDRON, PYRAMID, PRISM, 0, HEXAHEDRON };

    class VTUHybridOutputImpl
        : public geode::detail::VTUOutputImpl< geode::HybridSolid >
    {
    public:
        VTUHybridOutputImpl(
            absl::string_view filename, const geode::HybridSolid3D& solid )
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
            const auto vtk_type = VTK_CELL_TYPE[nb_vertices];
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
        void VTUHybridOutput::write( const HybridSolid3D& solid ) const
        {
            VTUHybridOutputImpl impl{ filename(), solid };
            impl.write_file();
        }
    } // namespace detail
} // namespace geode
