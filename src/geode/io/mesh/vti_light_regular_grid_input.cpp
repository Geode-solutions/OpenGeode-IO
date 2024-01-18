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

#include <geode/io/mesh/detail/vti_light_regular_grid_input.h>

#include <array>
#include <fstream>

#include <geode/basic/string.h>

#include <geode/mesh/core/light_regular_grid.h>

#include <geode/io/mesh/detail/vti_grid_input.h>

namespace
{
    template < geode::index_t dimension >
    class VTILightRegularGridInputImpl
        : public geode::detail::VTIGridInputImpl<
              geode::LightRegularGrid< dimension > >
    {
    public:
        VTILightRegularGridInputImpl( absl::string_view filename )
            : geode::detail::VTIGridInputImpl<
                geode::LightRegularGrid< dimension > >{ filename }
        {
        }

    private:
        void build_grid( const pugi::xml_node& vtk_object ) final
        {
            const auto grid_attributes =
                this->read_grid_attributes( vtk_object );
            this->initialize_mesh(
                absl::make_unique< geode::LightRegularGrid< dimension > >(
                    grid_attributes.origin, grid_attributes.cells_number,
                    grid_attributes.cells_length ) );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        LightRegularGrid< dimension >
            VTILightRegularGridInput< dimension >::read()
        {
            VTILightRegularGridInputImpl< dimension > reader{
                this->filename()
            };
            return std::move( *reader.read_file().release() );
        }

        template < index_t dimension >
        bool VTILightRegularGridInput< dimension >::is_loadable() const
        {
            return VTIGridInputImpl< LightRegularGrid< dimension > >::
                is_loadable( this->filename() );
        }

        template class VTILightRegularGridInput< 2 >;
        template class VTILightRegularGridInput< 3 >;
    } // namespace detail
} // namespace geode
