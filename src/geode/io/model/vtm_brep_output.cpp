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

#include <geode/io/model/detail/vtm_brep_output.hpp>

#include <string>
#include <vector>

#include <async++.h>

#include <absl/strings/str_cat.h>

#include <geode/basic/uuid.hpp>

#include <geode/mesh/core/hybrid_solid.hpp>
#include <geode/mesh/core/polyhedral_solid.hpp>
#include <geode/mesh/core/regular_grid_solid.hpp>
#include <geode/mesh/core/tetrahedral_solid.hpp>
#include <geode/mesh/io/hybrid_solid_output.hpp>
#include <geode/mesh/io/polyhedral_solid_output.hpp>
#include <geode/mesh/io/regular_grid_output.hpp>
#include <geode/mesh/io/tetrahedral_solid_output.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/representation/core/brep.hpp>

#include <geode/io/model/detail/vtm_output.hpp>

namespace
{
    class VTMBRepOutputImpl
        : public geode::detail::VTMOutputImpl< geode::BRep, 3 >
    {
    public:
        VTMBRepOutputImpl( std::string_view filename, const geode::BRep& brep )
            : geode::detail::VTMOutputImpl< geode::BRep, 3 >{ filename, brep }
        {
        }

    private:
        void write_piece( pugi::xml_node& object ) final
        {
            const auto counter = this->write_corners_lines_surfaces( object );
            auto block_block = object.append_child( "Block" );
            block_block.append_attribute( "name" ).set_value( "blocks" );
            block_block.append_attribute( "index" ).set_value( counter );
            write_blocks( block_block );
        }

        void write_blocks( pugi::xml_node& block_block )
        {
            geode::index_t counter{ 0 };
            const auto level = geode::Logger::level();
            geode::Logger::set_level( geode::Logger::LEVEL::warn );
            absl::FixedArray< async::task< void > > tasks( mesh().nb_blocks() );
            absl::FixedArray< geode::uuid > block_ids(
                this->mesh().nb_blocks() );
            geode::index_t block_count{ 0 };
            for( const auto& block : mesh().blocks() )
            {
                block_ids[block_count++] = block.id();
            }
            absl::c_sort( block_ids );
            absl::flat_hash_map< std::string, geode::index_t >
                block_name_counter;
            for( const auto& id : block_ids )
            {
                const auto& block = mesh().block( id );
                auto dataset = block_block.append_child( "DataSet" );
                dataset.append_attribute( "index" ).set_value( counter );
                const auto filename = absl::StrCat(
                    prefix(), "/Block_", block.id().string(), ".vtu" );
                dataset.append_attribute( "file" ).set_value(
                    filename.c_str() );
                if( !block_name_counter.contains( block.name() ) )
                {
                    dataset.append_attribute( "name" ).set_value(
                        std::string( block.name() ).c_str() );
                    block_name_counter.emplace( block.name(), 1 );
                }
                else
                {
                    dataset.append_attribute( "name" ).set_value(
                        absl::StrCat( block.name(), "_", counter ).c_str() );
                    block_name_counter[block.name()]++;
                }
                dataset.append_attribute( "uuid" ).set_value(
                    block.id().string().c_str() );
                tasks[counter++] = async::spawn( [&block, this] {
                    const auto& mesh = block.mesh();
                    const auto file = absl::StrCat(
                        files_directory(), "/Block_", block.id().string() );
                    if( const auto* tetra =
                            dynamic_cast< const geode::TetrahedralSolid3D* >(
                                &mesh ) )
                    {
                        geode::save_tetrahedral_solid(
                            *tetra, absl::StrCat( file, ".vtu" ) );
                    }
                    else if( const auto* hybrid =
                                 dynamic_cast< const geode::HybridSolid3D* >(
                                     &mesh ) )
                    {
                        geode::save_hybrid_solid(
                            *hybrid, absl::StrCat( file, ".vtu" ) );
                    }
                    else if( const auto* poly = dynamic_cast<
                                 const geode::PolyhedralSolid3D* >( &mesh ) )
                    {
                        geode::save_polyhedral_solid(
                            *poly, absl::StrCat( file, ".vtu" ) );
                    }
                    else if( const auto* grid =
                                 dynamic_cast< const geode::RegularGrid3D* >(
                                     &mesh ) )
                    {
                        geode::save_regular_grid(
                            *grid, absl::StrCat( file, ".vti" ) );
                    }
                    else
                    {
                        throw geode::OpenGeodeException(
                            "[Blocks::save_blocks] Cannot find the explicit "
                            "SolidMesh type." );
                    }
                } );
            }
            auto all_tasks = async::when_all( tasks.begin(), tasks.end() );
            all_tasks.wait();
            geode::Logger::set_level( level );
            for( auto& task : all_tasks.get() )
            {
                task.get();
            }
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        std::vector< std::string > VTMBRepOutput::write(
            const BRep& brep ) const
        {
            VTMBRepOutputImpl impl{ filename(), brep };
            impl.write_file();
            return impl.files();
        }
    } // namespace detail
} // namespace geode
