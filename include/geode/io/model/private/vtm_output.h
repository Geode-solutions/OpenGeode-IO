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

#pragma once

#include <async++.h>

#include <ghc/filesystem.hpp>

#include <geode/basic/filename.h>

#include <geode/mesh/core/edged_curve.h>
#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/polygonal_surface.h>
#include <geode/mesh/core/regular_grid_surface.h>
#include <geode/mesh/core/triangulated_surface.h>
#include <geode/mesh/io/edged_curve_output.h>
#include <geode/mesh/io/point_set_output.h>
#include <geode/mesh/io/polygonal_surface_output.h>
#include <geode/mesh/io/regular_grid_output.h>
#include <geode/mesh/io/triangulated_surface_output.h>

#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/mixin/core/surface.h>

#include <geode/io/image/private/vtk_output.h>

namespace geode
{
    namespace detail
    {
        template < typename Model, index_t dimension >
        class VTMOutputImpl : public VTKOutputImpl< Model >
        {
        public:
            VTMOutputImpl( absl::string_view filename, const Model& brep )
                : VTKOutputImpl< Model >{ filename, brep,
                      "vtkMultiBlockDataSet" },
                  files_directory_{ filename_without_extension( filename ) }
            {
                if( ghc::filesystem::path{ to_string( filename ) }
                        .is_relative() )
                {
                    const ghc::filesystem::path files_directory =
                        ghc::filesystem::current_path()
                        / filepath_without_extension( filename );
                    ghc::filesystem::create_directory( files_directory );
                }
                else
                {
                    ghc::filesystem::create_directory(
                        filepath_without_extension( filename ) );
                }
            }

        protected:
            index_t write_corners_lines_surfaces( pugi::xml_node& object ) const
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

            absl::string_view files_directory() const
            {
                return files_directory_;
            }

        private:
            void write_corners( pugi::xml_node& corner_block ) const
            {
                index_t counter{ 0 };
                const auto prefix =
                    absl::StrCat( files_directory_, "/Corner_" );
                const auto level = Logger::level();
                Logger::set_level( Logger::Level::warn );
                absl::FixedArray< async::task< void > > tasks(
                    this->mesh().nb_corners() );
                for( const auto& corner : this->mesh().corners() )
                {
                    auto dataset = corner_block.append_child( "DataSet" );
                    dataset.append_attribute( "index" ).set_value( counter );
                    const auto filename =
                        absl::StrCat( prefix, corner.id().string(), ".vtp" );
                    dataset.append_attribute( "file" ).set_value(
                        filename.c_str() );

                    tasks[counter++] = async::spawn( [&corner, &prefix] {
                        const auto& mesh = corner.mesh();
                        const auto file = absl::StrCat(
                            prefix, corner.id().string(), ".vtp" );
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

            void write_lines( pugi::xml_node& line_block ) const
            {
                index_t counter{ 0 };
                const auto prefix = absl::StrCat( files_directory_, "/Line_" );
                const auto level = Logger::level();
                Logger::set_level( Logger::Level::warn );
                absl::FixedArray< async::task< void > > tasks(
                    this->mesh().nb_lines() );
                for( const auto& line : this->mesh().lines() )
                {
                    auto dataset = line_block.append_child( "DataSet" );
                    dataset.append_attribute( "index" ).set_value( counter );
                    const auto filename =
                        absl::StrCat( prefix, line.id().string(), ".vtp" );
                    dataset.append_attribute( "file" ).set_value(
                        filename.c_str() );

                    tasks[counter++] = async::spawn( [&line, &prefix] {
                        const auto& mesh = line.mesh();
                        const auto file =
                            absl::StrCat( prefix, line.id().string(), ".vtp" );
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

            void write_surfaces( pugi::xml_node& surface_block ) const
            {
                index_t counter{ 0 };
                const auto prefix =
                    absl::StrCat( files_directory_, "/Surface_" );
                const auto level = Logger::level();
                Logger::set_level( Logger::Level::warn );
                absl::FixedArray< async::task< void > > tasks(
                    this->mesh().nb_surfaces() );
                for( const auto& surface : this->mesh().surfaces() )
                {
                    auto dataset = surface_block.append_child( "DataSet" );
                    dataset.append_attribute( "index" ).set_value( counter );
                    const auto filename =
                        absl::StrCat( prefix, surface.id().string(), ".vtp" );
                    dataset.append_attribute( "file" ).set_value(
                        filename.c_str() );

                    tasks[counter++] = async::spawn( [&surface, &prefix] {
                        const auto& mesh = surface.mesh();
                        const auto file = absl::StrCat(
                            prefix, surface.id().string(), ".vtp" );
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
        };
    } // namespace detail
} // namespace geode