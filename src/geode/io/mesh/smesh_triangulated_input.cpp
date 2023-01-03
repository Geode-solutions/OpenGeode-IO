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

#include <geode/io/mesh/private/smesh_triangulated_input.h>

#include <fstream>

#include <absl/strings/str_split.h>

#include <geode/basic/filename.h>

#include <geode/geometry/point.h>

#include <geode/mesh/builder/triangulated_surface_builder.h>
#include <geode/mesh/core/triangulated_surface.h>

#include <geode/io/mesh/private/smesh_input.h>

namespace
{
    class SMESHTriangulatedInputImpl
        : public geode::detail::SMESHInputImpl< geode::TriangulatedSurface3D,
              geode::TriangulatedSurfaceBuilder3D,
              3 >
    {
    public:
        SMESHTriangulatedInputImpl( absl::string_view filename,
            geode::TriangulatedSurface3D& triangulated_surface )
            : geode::detail::SMESHInputImpl< geode::TriangulatedSurface3D,
                geode::TriangulatedSurfaceBuilder3D,
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
    namespace detail
    {
        std::unique_ptr< TriangulatedSurface3D > SMESHTriangulatedInput::read(
            const MeshImpl& impl )
        {
            auto surface = TriangulatedSurface3D::create( impl );
            SMESHTriangulatedInputImpl reader{ filename(), *surface };
            reader.read_file();
            return surface;
        }
    } // namespace detail
} // namespace geode
