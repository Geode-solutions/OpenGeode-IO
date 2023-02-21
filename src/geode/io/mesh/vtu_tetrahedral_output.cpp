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

#include <geode/io/mesh/private/vtu_tetrahedral_output.h>

#include <geode/mesh/core/tetrahedral_solid.h>

#include <geode/io/mesh/private/vtu_output_impl.h>

namespace
{
    class VTUTetrahedralOutputImpl
        : public geode::detail::VTUOutputImpl< geode::TetrahedralSolid >
    {
    public:
        VTUTetrahedralOutputImpl(
            absl::string_view filename, const geode::TetrahedralSolid3D& solid )
            : geode::detail::VTUOutputImpl< geode::TetrahedralSolid >{ filename,
                  solid }
        {
        }

    private:
        void write_cell( geode::index_t /*unused*/,
            std::string& cell_types,
            std::string& /*unused*/,
            std::string& /*unused*/,
            geode::index_t& /*unused*/ ) const override
        {
            absl::StrAppend( &cell_types, "10 " );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void VTUTetrahedralOutput::write(
            const TetrahedralSolid3D& solid ) const
        {
            VTUTetrahedralOutputImpl impl{ filename(), solid };
            impl.write_file();
        }
    } // namespace detail
} // namespace geode
