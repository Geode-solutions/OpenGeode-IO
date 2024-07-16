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

#include <geode/io/mesh/detail/vti_light_regular_grid_output.hpp>

#include <string>

#include <geode/geometry/coordinate_system.hpp>

#include <geode/mesh/core/light_regular_grid.hpp>

#include <geode/io/mesh/detail/vti_grid_output.hpp>

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        std::vector< std::string >
            VTILightRegularGridOutput< dimension >::write(
                const LightRegularGrid< dimension >& grid ) const
        {
            VTIGridOutputImpl< dimension > impl{ grid, this->filename() };
            impl.write_file();
            return { to_string( this->filename() ) };
        }

        template class VTILightRegularGridOutput< 2 >;
        template class VTILightRegularGridOutput< 3 >;
    } // namespace detail
} // namespace geode
