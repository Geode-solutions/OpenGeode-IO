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

#include <geode/io/mesh/internal/smesh_triangulated_input.hpp>

#include <fstream>

#include <absl/strings/str_split.h>

#include <geode/basic/filename.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/triangulated_surface_builder.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>

#include <geode/io/mesh/internal/smesh_input.hpp>

namespace
{
    class SMESHTriangulatedInputImpl
        : public geode::internal::SMESHInputImpl< geode::TriangulatedSurface3D,
              3 >
    {
    public:
        SMESHTriangulatedInputImpl( std::string_view filename,
            geode::TriangulatedSurface3D& triangulated_surface )
            : geode::internal::SMESHInputImpl< geode::TriangulatedSurface3D,
                  3 >( filename, triangulated_surface )
        {
        }

    private:
        void create_element(
            const std::array< geode::index_t, 3 >& vertices ) override
        {
            builder().create_triangle( vertices );
        }
    };
} // namespace

namespace geode
{
    namespace internal
    {
        std::unique_ptr< TriangulatedSurface3D > SMESHTriangulatedInput::read(
            const MeshImpl& impl )
        {
            auto surface = TriangulatedSurface3D::create( impl );
            SMESHTriangulatedInputImpl reader{ filename(), *surface };
            reader.read_file();
            return surface;
        }

        Percentage SMESHTriangulatedInput::is_loadable() const
        {
            auto curve = EdgedCurve3D::create();
            SMESHCurveInputImpl reader{ filename(), *curve };
            return reader.is_loadable();
        }
    } // namespace internal
} // namespace geode
