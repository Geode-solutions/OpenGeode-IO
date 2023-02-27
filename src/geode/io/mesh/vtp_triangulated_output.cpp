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

#include <geode/io/mesh/private/vtp_triangulated_output.h>

#include <geode/mesh/core/triangulated_surface.h>

#include <geode/io/mesh/private/vtp_surface_output_impl.h>

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        void VTPTriangulatedOutput< dimension >::write(
            const TriangulatedSurface< dimension > &surface ) const
        {
            VTPSurfaceOutputImpl< TriangulatedSurface, dimension > impl{
                this->filename(), surface
            };
            impl.write_file();
        }

        template class VTPTriangulatedOutput< 2 >;
        template class VTPTriangulatedOutput< 3 >;
    } // namespace detail
} // namespace geode
