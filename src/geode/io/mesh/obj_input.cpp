/*
 * Copyright (c) 2019 - 2021 Geode-solutions
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

#include <geode/io/mesh/private/obj_input.h>

#include <geode/mesh/core/polygonal_surface.h>

#include <geode/io/mesh/private/assimp_input.h>

namespace
{
    class OBJInputImpl : public geode::detail::AssimpMeshInput
    {
    public:
        OBJInputImpl( absl::string_view filename,
            geode::PolygonalSurface3D& polygonal_surface )
            : geode::detail::AssimpMeshInput( filename ),
              surface_( polygonal_surface )
        {
        }

        void build_mesh()
        {
            build_mesh_from_duplicated_vertices( surface_ );
        }

    private:
        geode::PolygonalSurface3D& surface_;
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void OBJInput::do_read()
        {
            OBJInputImpl impl{ filename(), polygonal_surface() };
            impl.read_file();
            impl.build_mesh();
        }
    } // namespace detail
} // namespace geode