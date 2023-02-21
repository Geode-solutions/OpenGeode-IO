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

#include <geode/io/mesh/private/vtu_tetrahedral_input.h>

#include <geode/mesh/builder/tetrahedral_solid_builder.h>
#include <geode/mesh/core/tetrahedral_solid.h>

#include <geode/io/mesh/private/vtu_input_impl.h>

namespace
{
    class VTUTetrahedralInputImpl
        : public geode::detail::VTUInputImpl< geode::TetrahedralSolid3D,
              geode::TetrahedralSolidBuilder3D >
    {
        using VTKElement = absl::FixedArray< std::vector< geode::index_t > >;

    public:
        VTUTetrahedralInputImpl(
            absl::string_view filename, geode::TetrahedralSolid3D& solid )
            : geode::detail::VTUInputImpl< geode::TetrahedralSolid3D,
                geode::TetrahedralSolidBuilder3D >( filename, solid )
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
            auto solid = TetrahedralSolid3D::create( impl );
            VTUTetrahedralInputImpl reader{ filename(), *solid };
            reader.read_file();
            return solid;
        }
    } // namespace detail
} // namespace geode
