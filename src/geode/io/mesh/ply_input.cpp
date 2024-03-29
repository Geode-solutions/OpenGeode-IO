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

#include <geode/io/mesh/private/ply_input.h>

#include <geode/geometry/nn_search.h>
#include <geode/geometry/point.h>

#include <geode/mesh/builder/polygonal_surface_builder.h>
#include <geode/mesh/core/polygonal_surface.h>

#include <geode/io/mesh/private/assimp_input.h>

namespace geode
{
    namespace detail
    {
        std::unique_ptr< PolygonalSurface3D > PLYInput::read(
            const MeshImpl& /*unused*/ )
        {
            geode::detail::AssimpMeshInput< geode::PolygonalSurface3D > reader{
                filename()
            };
            return reader.read_file();
        }
    } // namespace detail
} // namespace geode
