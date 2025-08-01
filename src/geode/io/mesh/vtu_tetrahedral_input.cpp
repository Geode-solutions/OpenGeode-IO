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

#include <geode/io/mesh/detail/vtu_tetrahedral_input.hpp>

#include <geode/mesh/builder/tetrahedral_solid_builder.hpp>
#include <geode/mesh/core/mesh_factory.hpp>
#include <geode/mesh/core/tetrahedral_solid.hpp>

#include <geode/io/mesh/detail/vtu_solid_input.hpp>

namespace
{
    class VTUTetrahedralInputImpl
        : public geode::detail::VTUSolidInput< geode::TetrahedralSolid3D >
    {
        using VTKElement = absl::FixedArray< std::vector< geode::index_t > >;

    public:
        VTUTetrahedralInputImpl(
            std::string_view filename, const geode::MeshImpl& impl )
            : geode::detail::VTUSolidInput< geode::TetrahedralSolid3D >(
                  filename, impl )
        {
            enable_tetrahedron();
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        std::unique_ptr< TetrahedralSolid3D > VTUTetrahedralInput::read(
            const MeshImpl& impl )
        {
            VTUTetrahedralInputImpl reader{ filename(), impl };
            return reader.read_file();
        }

        Percentage VTUTetrahedralInput::is_loadable() const
        {
            VTUTetrahedralInputImpl reader{ filename(),
                MeshFactory::default_impl(
                    TetrahedralSolid3D::type_name_static() ) };
            return reader.is_loadable();
        }
    } // namespace detail
} // namespace geode
