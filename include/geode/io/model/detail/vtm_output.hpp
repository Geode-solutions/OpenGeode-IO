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

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <async++.h>

#include <absl/container/fixed_array.h>

#include <geode/basic/filename.hpp>
#include <geode/basic/uuid.hpp>

#include <geode/mesh/core/edged_curve.hpp>
#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/core/regular_grid_surface.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>
#include <geode/mesh/io/edged_curve_output.hpp>
#include <geode/mesh/io/point_set_output.hpp>
#include <geode/mesh/io/polygonal_surface_output.hpp>
#include <geode/mesh/io/regular_grid_output.hpp>
#include <geode/mesh/io/triangulated_surface_output.hpp>

#include <geode/model/mixin/core/corner.hpp>
#include <geode/model/mixin/core/line.hpp>
#include <geode/model/mixin/core/surface.hpp>

#include <geode/io/image/detail/vtk_output.hpp>

namespace geode
{
    namespace detail
    {
        template < typename Model, index_t dimension >
        class VTMOutputImpl : public VTKOutputImpl< Model >
        {
        public:
            VTMOutputImpl( std::string_view filename, const Model& brep )
                : VTKOutputImpl< Model >{ filename, brep,
                      "vtkMultiBlockDataSet" },
                  files_directory_{
                      filepath_without_extension( filename ).string()
                  },
                  prefix_{ filename_without_extension( filename ).string() }
            {
                if( std::filesystem::path{ to_string( filename ) }
                        .is_relative() )
                {
                    std::filesystem::create_directory(
                        std::filesystem::current_path() / files_directory_ );
                }
                else
                {
                    std::filesystem::create_directory( files_directory_ );
                }
                add_file( to_string( filename ) );
            }

            std::vector< std::string > files()
            {
                return std::move( files_ );
            }

        protected:
            index_t write_corners_lines_surfaces( pugi::xml_node& object )
            {
                index_t counter{ 0 };
                auto corner_block = object.append_child( "Block" );
                corner_block.append_attribute( "name" ).set_value( "corners" );
                corner_block.append_attribute( "index" ).set_value( counter++ );
                write_corners( corner_block );
                auto line_block = object.append_child( "Block" );
                line_block.append_attribute( "name" ).set_value( "lines" );
                line_block.append_attribute( "index" ).set_value( counter++ );
                write_lines( line_block );
                auto surface_block = object.append_child( "Block" );
                surface_block.append_attribute( "name" ).set_value(
                    "surfaces" );
                surface_block.append_attribute( "index" ).set_value(
                    counter++ );
                write_surfaces( surface_block );
                return counter;
            }

            std::string_view prefix() const
            {
                return prefix_;
            }

            std::string_view files_directory() const
            {
                return files_directory_;
            }

            void add_file( const std::string& filename )
            {
                files_.emplace_back( filename );
            }

        private:
            void write_corners( pugi::xml_node& corner_block )
            {
                index_t counter{ 0 };
                const auto level = Logger::level();
                Logger::set_level( Logger::LEVEL::warn );
                absl::FixedArray< async::task< void > > tasks(
                    this->mesh().nb_corners() );
                absl::FixedArray< uuid > corner_ids(
                    this->mesh().nb_corners() );
                index_t corner_count{ 0 };
                for( const auto& corner : this->mesh().corners() )
                {
                    corner_ids[corner_count++] = corner.id();
                }
                absl::c_sort( corner_ids );
                for( const auto& id : corner_ids )
                {
                    const auto& corner = this->mesh().corner( id );
                    auto dataset = corner_block.append_child( "DataSet" );
                    dataset.append_attribute( "index" ).set_value( counter );
                    const auto filename = absl::StrCat(
                        prefix_, "/Corner_", corner.id().string(), ".vtp" );
                    dataset.append_attribute( "file" ).set_value(
                        filename.c_str() );

                    tasks[counter++] = async::spawn( [&corner, this] {
                        const auto& mesh = corner.mesh();
                        const auto file = absl::StrCat( files_directory_,
                            "/Corner_", corner.id().string(), ".vtp" );
                        save_point_set( mesh, file );
                    } );
                }
                auto all_tasks = async::when_all( tasks.begin(), tasks.end() );
                all_tasks.wait();
                Logger::set_level( level );
                for( auto& task : all_tasks.get() )
                {
                    task.get();
                }
            }

            void write_lines( pugi::xml_node& line_block )
            {
                index_t counter{ 0 };
                const auto level = Logger::level();
                Logger::set_level( Logger::LEVEL::warn );
                absl::FixedArray< async::task< void > > tasks(
                    this->mesh().nb_lines() );
                absl::FixedArray< uuid > line_ids( this->mesh().nb_lines() );
                index_t line_count{ 0 };
                for( const auto& line : this->mesh().lines() )
                {
                    line_ids[line_count++] = line.id();
                }
                absl::c_sort( line_ids );
                for( const auto& id : line_ids )
                {
                    const auto& line = this->mesh().line( id );
                    auto dataset = line_block.append_child( "DataSet" );
                    dataset.append_attribute( "index" ).set_value( counter );
                    const auto filename = absl::StrCat(
                        prefix_, "/Line_", line.id().string(), ".vtp" );
                    dataset.append_attribute( "file" ).set_value(
                        filename.c_str() );

                    tasks[counter++] = async::spawn( [&line, this] {
                        const auto& mesh = line.mesh();
                        const auto file = absl::StrCat( files_directory_,
                            "/Line_", line.id().string(), ".vtp" );
                        save_edged_curve( mesh, file );
                    } );
                }
                auto all_tasks = async::when_all( tasks.begin(), tasks.end() );
                all_tasks.wait();
                Logger::set_level( level );
                for( auto& task : all_tasks.get() )
                {
                    task.get();
                }
            }

            void write_surfaces( pugi::xml_node& surface_block )
            {
                index_t counter{ 0 };
                const auto level = Logger::level();
                Logger::set_level( Logger::LEVEL::warn );
                absl::FixedArray< async::task< void > > tasks(
                    this->mesh().nb_surfaces() );
                absl::FixedArray< uuid > surface_ids(
                    this->mesh().nb_surfaces() );
                index_t surface_count{ 0 };
                for( const auto& surface : this->mesh().surfaces() )
                {
                    surface_ids[surface_count++] = surface.id();
                }
                absl::c_sort( surface_ids );
                for( const auto& id : surface_ids )
                {
                    const auto& surface = this->mesh().surface( id );
                    auto dataset = surface_block.append_child( "DataSet" );
                    dataset.append_attribute( "index" ).set_value( counter );
                    const auto filename = absl::StrCat(
                        prefix_, "/Surface_", surface.id().string(), ".vtp" );
                    dataset.append_attribute( "file" ).set_value(
                        filename.c_str() );

                    tasks[counter++] = async::spawn( [&surface, this] {
                        const auto& mesh = surface.mesh();
                        const auto file = absl::StrCat( files_directory_,
                            "/Surface_", surface.id().string(), ".vtp" );
                        if( const auto* triangulated = dynamic_cast<
                                const TriangulatedSurface< dimension >* >(
                                &mesh ) )
                        {
                            save_triangulated_surface( *triangulated, file );
                        }
                        else if( const auto* polygonal = dynamic_cast<
                                     const PolygonalSurface< dimension >* >(
                                     &mesh ) )
                        {
                            save_polygonal_surface( *polygonal, file );
                        }
                        else if( const auto* grid =
                                     dynamic_cast< const RegularGrid2D* >(
                                         &mesh ) )
                        {
                            save_regular_grid( *grid, file );
                        }
                        else
                        {
                            throw OpenGeodeException(
                                "[Surfaces::save_surfaces] Cannot find the "
                                "explicit SurfaceMesh type" );
                        }
                    } );
                }
                auto all_tasks = async::when_all( tasks.begin(), tasks.end() );
                all_tasks.wait();
                Logger::set_level( level );
                for( auto& task : all_tasks.get() )
                {
                    task.get();
                }
            }

        private:
            DEBUG_CONST std::string files_directory_;
            std::vector< std::string > files_;
            DEBUG_CONST std::string prefix_;
        };
    } // namespace detail
} // namespace geode