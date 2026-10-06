/*
 * Copyright (c) 2019 - 2026 Geode-solutions
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

#include <geode/tests_config.hpp>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>

#include <absl/strings/str_cat.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/attribute_time_series.hpp>
#include <geode/basic/logger.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/solid_mesh_builder.hpp>
#include <geode/mesh/core/solid_mesh.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/representation/builder/brep_builder.hpp>
#include <geode/model/representation/core/brep.hpp>
#include <geode/model/representation/io/brep_input.hpp>
#include <geode/model/representation/io/brep_output.hpp>
#include <geode/model/representation/io/brep_time_series_input.hpp>

#include <geode/io/time_series/common.hpp>

namespace
{
    constexpr geode::index_t NB_CELLS{ 26400 };

    constexpr std::array< double, 5 > TIMES{ 0., 5e5, 1e6, 1.5e6, 2e6 };
    constexpr std::array< double, 5 > MIN_PRESSURES{ 41369000, 27579000,
        27579000, 27579000, 27579000 };
    constexpr std::array< double, 5 > MAX_PRESSURES{ 41369000,
        317036303.68696225, 325747376.2510222, 317089362.62659305,
        288502139.3156352 };
    constexpr double TEMPERATURE{ 300 };

    // GEOS simulation of the SPE10 benchmark (layers 84 and 85) run on
    // the grid of the BRep
    std::string simulation_file( std::string_view filename )
    {
        return absl::StrCat( geode::DATA_PATH, "spe10/", filename );
    }

    geode::BRep load_model()
    {
        return geode::load_brep(
            simulation_file( "grid_geos_with_physical_properties.og_brep" ) );
    }

    void check_series_steps(
        const geode::AttributeTimeSeries< double >& series )
    {
        geode::OpenGeodeIOTimeSeriesException::test(
            series.nb_time_steps() == TIMES.size(),
            "[Test] Wrong number of time steps" );
        for( const auto step : geode::LRange{ TIMES.size() } )
        {
            geode::OpenGeodeIOTimeSeriesException::test(
                series.time( step ) == TIMES[step], "[Test] Wrong time" );
        }
    }

    void check_block( const geode::Block3D& block )
    {
        const auto& mesh = block.mesh();
        const auto& cell_manager = mesh.polyhedron_attribute_manager();
        const geode::AttributeTimeSeries< double > pressure{ cell_manager,
            "pressure" };
        check_series_steps( pressure );
        const geode::AttributeTimeSeries< double > temperature{ cell_manager,
            "temperature" };
        check_series_steps( temperature );
        for( const auto step : geode::LRange{ TIMES.size() } )
        {
            auto min_pressure = pressure.value( step, 0 );
            auto max_pressure = min_pressure;
            for( const auto p : geode::Range{ mesh.nb_polyhedra() } )
            {
                const auto value = pressure.value( step, p );
                min_pressure = std::min( min_pressure, value );
                max_pressure = std::max( max_pressure, value );
                geode::OpenGeodeIOTimeSeriesException::test(
                    temperature.value( step, p ) == TEMPERATURE,
                    "[Test] Wrong temperature value" );
            }
            geode::OpenGeodeIOTimeSeriesException::test(
                min_pressure == MIN_PRESSURES[step],
                "[Test] Wrong minimum pressure at step ", step );
            geode::OpenGeodeIOTimeSeriesException::test(
                max_pressure == MAX_PRESSURES[step],
                "[Test] Wrong maximum pressure at step ", step );
        }
        const std::array< const geode::AttributeManager*, 2 > managers{
            &cell_manager, &mesh.vertex_attribute_manager()
        };
        for( const auto* manager : managers )
        {
            for( const auto name : { "localToGlobalMap", "ghostRank" } )
            {
                geode::OpenGeodeIOTimeSeriesException::test(
                    !manager->attribute_ids_matching_name( name ), "[Test] ",
                    name, " should not be imported" );
            }
        }
    }

    void check_brep( const geode::BRep& brep )
    {
        geode::index_t nb_cells{ 0 };
        for( const auto& block : brep.blocks() )
        {
            check_block( block );
            nb_cells += block.mesh().nb_polyhedra();
        }
        geode::OpenGeodeIOTimeSeriesException::test(
            nb_cells == NB_CELLS, "[Test] Wrong number of Block cells" );
    }

    void test_import()
    {
        auto brep = load_model();
        const auto pvd = simulation_file( "vtkOutput.pvd" );
        geode::OpenGeodeIOTimeSeriesException::test(
            geode::is_brep_time_series_loadable( pvd ).value() == 1,
            "[Test] Collection should be loadable" );
        geode::OpenGeodeIOTimeSeriesException::test(
            !geode::brep_time_series_additional_files( pvd )
                .has_additional_files(),
            "[Test] Collection should have no missing file" );

        geode::load_brep_time_series( brep, pvd );
        geode::save_brep( brep, "test_import.og_brep" );
        check_brep( brep );
        geode::load_brep_time_series( brep, pvd );
        check_brep( brep );
    }

    void test_missing_file()
    {
        const std::string pvd{ "pvd_input/missing.pvd" };
        std::ofstream file{ pvd };
        file << "<?xml version=\"1.0\"?>\n<VTKFile type=\"Collection\" "
                "version=\"0.1\">\n<Collection>\n<DataSet timestep=\"0\" "
                "file=\""
             << simulation_file( "vtkOutput/000000.vtm" )
             << "\" />\n<DataSet timestep=\"1\" "
                "file=\"missing/step_1.vtm\" />\n</Collection>\n</VTKFile>\n";
        file.close();
        const auto loadable =
            geode::is_brep_time_series_loadable( pvd ).value();
        geode::OpenGeodeIOTimeSeriesException::test(
            loadable > 0 && loadable < 1,
            "[Test] Collection with a missing file should be partially "
            "loadable" );
        geode::OpenGeodeIOTimeSeriesException::test(
            geode::brep_time_series_additional_files( pvd )
                .has_additional_files(),
            "[Test] Collection should have a missing file" );
    }

} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOTimeSeriesLibrary::initialize();
        std::filesystem::create_directories( "pvd_input" );
        test_import();
        test_missing_file();
        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
