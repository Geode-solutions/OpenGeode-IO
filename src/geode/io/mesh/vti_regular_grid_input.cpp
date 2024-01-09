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

#include <geode/io/mesh/private/vti_regular_grid_input.h>

#include <array>
#include <fstream>

#include <geode/basic/string.h>

#include <geode/mesh/builder/regular_grid_solid_builder.h>
#include <geode/mesh/builder/regular_grid_surface_builder.h>
#include <geode/mesh/core/regular_grid_solid.h>
#include <geode/mesh/core/regular_grid_surface.h>

#include <geode/io/mesh/private/vti_grid_input.h>

namespace
{
    template < geode::index_t dimension >
    class VTIRegularGridInputImpl : public geode::detail::VTIGridInputImpl<
                                        geode::RegularGrid< dimension > >
    {
    public:
        VTIRegularGridInputImpl(
            absl::string_view filename, const geode::MeshImpl& impl )
            : geode::detail::VTIGridInputImpl<
                geode::RegularGrid< dimension > >{ filename }
        {
            this->initialize_mesh(
                geode::RegularGrid< dimension >::create( impl ) );
        }

    private:
        void build_grid( const pugi::xml_node& vtk_object ) final
        {
            const auto grid_attributes =
                this->read_grid_attributes( vtk_object );
            auto builder = geode::RegularGrid< dimension >::Builder::create(
                this->mesh() );
            builder->initialize_grid( grid_attributes.origin,
                grid_attributes.cells_number, grid_attributes.cell_directions );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        template < index_t dimension >
        std::unique_ptr< RegularGrid< dimension > >
            VTIRegularGridInput< dimension >::read( const MeshImpl& impl )
        {
            VTIRegularGridInputImpl< dimension > reader{ this->filename(),
                impl };
            return reader.read_file();
        }

        template < index_t dimension >
        bool VTIRegularGridInput< dimension >::is_loadable() const
        {
            return VTIGridInputImpl< RegularGrid< dimension > >::is_loadable(
                this->filename() );
        }

        template class VTIRegularGridInput< 2 >;
        template class VTIRegularGridInput< 3 >;
    } // namespace detail
} // namespace geode
