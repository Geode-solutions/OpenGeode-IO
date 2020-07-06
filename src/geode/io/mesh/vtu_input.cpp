/*
 * Copyright (c) 2019 - 2020 Geode-solutions
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

#include <geode/io/mesh/private/vtu_input.h>

#include <geode/mesh/builder/polyhedral_solid_builder.h>
#include <geode/mesh/core/polyhedral_solid.h>

#include <geode/io/mesh/private/vtk_input.h>

namespace
{
    class VTUInputImpl
        : public geode::detail::VTKInputImpl< geode::PolyhedralSolid3D,
              geode::PolyhedralSolidBuilder3D >
    {
        using VTKElement = absl::FixedArray< std::vector< geode::index_t > >;

    public:
        VTUInputImpl(
            absl::string_view filename, geode::PolyhedralSolid3D& solid )
            : geode::detail::VTKInputImpl< geode::PolyhedralSolid3D,
                  geode::PolyhedralSolidBuilder3D >(
                  filename, solid, "UnstructuredGrid" )
        {
            VTKElement tetra( 4 );
            tetra[0] = { 1, 3, 2 };
            tetra[1] = { 0, 2, 3 };
            tetra[2] = { 3, 1, 0 };
            tetra[3] = { 0, 1, 2 };
            elements_.emplace( 10, std::move( tetra ) );
            VTKElement hexa( 6 );
            hexa[0] = { 0, 4, 5, 1 };
            hexa[1] = { 1, 5, 7, 3 };
            hexa[2] = { 3, 7, 6, 2 };
            hexa[3] = { 2, 6, 4, 0 };
            hexa[4] = { 4, 6, 7, 5 };
            hexa[5] = { 0, 1, 3, 2 };
            elements_.emplace( 11, hexa ); // voxel
            elements_.emplace( 12, std::move( hexa ) ); // hexa
            VTKElement prism( 5 );
            prism[0] = { 0, 2, 1 };
            prism[1] = { 3, 4, 5 };
            prism[2] = { 0, 3, 5, 2 };
            prism[3] = { 1, 2, 5, 4 };
            prism[4] = { 0, 1, 4, 3 };
            elements_.emplace( 13, std::move( prism ) );
            VTKElement pyramid( 5 );
            pyramid[0] = { 0, 4, 1 };
            pyramid[1] = { 1, 4, 2 };
            pyramid[2] = { 2, 4, 3 };
            pyramid[3] = { 3, 4, 0 };
            pyramid[4] = { 0, 1, 2, 3 };
            elements_.emplace( 14, std::move( pyramid ) );
        }

    private:
        void read_vtk_cells( const pugi::xml_node& piece ) override
        {
            const auto nb_polyhedra = read_attribute( piece, "NumberOfCells" );
            // read_cell_data( piece );
            const auto output = read_polyhedra( piece, nb_polyhedra );
            build_polyhedra( std::get< 0 >( output ), std::get< 1 >( output ) );
        }

        std::tuple< absl::FixedArray< std::vector< geode::index_t > >,
            std::vector< int64_t > >
            read_polyhedra(
                const pugi::xml_node& piece, geode::index_t nb_polyhedra )
        {
            std::vector< int64_t > offsets_values;
            std::vector< int64_t > connectivity_values;
            std::vector< int64_t > types_values;
            for( const auto& data :
                piece.child( "Cells" ).children( "DataArray" ) )
            {
                if( match( data.attribute( "Name" ).value(), "offsets" ) )
                {
                    offsets_values = read_data_array< int64_t >( data );
                    OPENGEODE_ASSERT( offsets_values.size() == nb_polyhedra,
                        "[VTKInput::read_polyhedra] Wrong number of offsets" );
                    geode_unused( nb_polyhedra );
                }
                else if( match( data.attribute( "Name" ).value(),
                             "connectivity" ) )
                {
                    connectivity_values = read_data_array< int64_t >( data );
                }
                else if( match( data.attribute( "Name" ).value(), "types" ) )
                {
                    types_values = read_data_array< int64_t >( data );
                    OPENGEODE_ASSERT( types_values.size() == nb_polyhedra,
                        "[VTKInput::read_polyhedra] Wrong number of types" );
                    geode_unused( nb_polyhedra );
                }
            }
            return std::make_tuple(
                get_cell_vertices( connectivity_values, offsets_values ),
                types_values );
        }

        void build_polyhedra( absl::Span< const std::vector< geode::index_t > >
                                  polyhedron_vertices,
            absl::Span< const int64_t > types )
        {
            absl::FixedArray< geode::index_t > new_polyhedra(
                polyhedron_vertices.size() );
            std::iota( new_polyhedra.begin(), new_polyhedra.end(),
                mesh().nb_polyhedra() );
            for( const auto p : geode::Range{ new_polyhedra.size() } )
            {
                try
                {
                    builder().create_polyhedron(
                        polyhedron_vertices[p], elements_.at( types[p] ) );
                }
                catch( const std::exception& /*unused*/ )
                {
                    geode::Logger::warn( "[VTKInput::build_polyhedra] Non "
                                         "polyhedra element is not supported" );
                }
            }
            builder().compute_polyhedron_adjacencies( new_polyhedra );
        }

    private:
        absl::flat_hash_map< int64_t, VTKElement > elements_;
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void VTUInput::do_read()
        {
            VTUInputImpl impl{ filename(), polyhedral_solid() };
            impl.read_file();
        }
    } // namespace detail
} // namespace geode
