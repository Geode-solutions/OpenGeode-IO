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
#include <limits>
#include <string>
#include <string_view>

#include <absl/strings/str_cat.h>
#include <absl/types/span.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/attribute_time_series.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/mesh/core/solid_mesh.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/mixin/core/physical_properties.hpp>
#include <geode/model/representation/core/brep.hpp>
#include <geode/model/representation/io/brep_input.hpp>
#include <geode/model/representation/io/brep_output.hpp>
#include <geode/model/representation/io/brep_time_series_input.hpp>

#include <geode/io/time_series/common.hpp>

namespace
{
    using Permeability = std::array< double, 3 >;

    constexpr geode::index_t NB_BLOCKS{ 7 };
    constexpr geode::index_t NB_CELLS{ 112913 };
    // 3 .vtm, each listing one .vtu per GEOS region
    constexpr geode::index_t NB_COLLECTION_FILES{ 9 };

    constexpr std::array< double, 3 > TIMES{ 0., 5e7, 1e8 };
    constexpr std::array< double, 3 > MIN_PRESSURES{ 3786217.756711456,
        4550413.529513211, 4579543.672818428 };
    constexpr double SOURCE_PRESSURE{ 1.5e8 };
    constexpr double SINK_PRESSURE{ 5e6 };
    constexpr geode::index_t NB_SOURCE_CELLS{ 12 };
    constexpr geode::index_t NB_SINK_CELLS{ 112 };

    struct StepPressures
    {
        double min{ std::numeric_limits< double >::max() };
        double max{ std::numeric_limits< double >::lowest() };
        geode::index_t nb_source_cells{ 0 };
        geode::index_t nb_sink_cells{ 0 };
    };
    using PressuresPerStep = std::array< StepPressures, TIMES.size() >;

    // GEOS single phase flow simulation run on the picasso BRep, whose
    // porosity and permeability are physical properties of its 7 Blocks.
    // The Blocks are exported in 2 GEOS regions (Reservoir and Burden): each
    // time step is made of 2 .vtu files.
    std::string simulation_file( std::string_view filename )
    {
        return absl::StrCat( geode::DATA_PATH, "picasso/", filename );
    }

    geode::BRep load_model()
    {
        return geode::load_brep(
            simulation_file( "picasso_with_physical_properties.og_brep" ) );
    }

    bool is_reservoir( const geode::Block3D& block )
    {
        return block.name() == "Region_1" || block.name() == "Region_2";
    }

    template < typename T >
    void check_series_steps( const geode::AttributeTimeSeries< T >& series )
    {
        geode::OpenGeodeIOTimeSeriesException::test(
            series.nb_time_steps() == TIMES.size(),
            "[Test] Wrong number of time steps" );
        for( const auto step : geode::LIndices{ TIMES } )
        {
            geode::OpenGeodeIOTimeSeriesException::test(
                series.time( step ) == TIMES[step], "[Test] Wrong time" );
        }
    }

    void check_ignored_attributes( const geode::SolidMesh3D& mesh )
    {
        const std::array< const geode::AttributeManager*, 2 > managers{
            &mesh.polyhedron_attribute_manager(),
            &mesh.vertex_attribute_manager()
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

    // GEOS porosity and permeability come from the Block physical
    // properties: equal values ensure each GEOS cell is matched with its
    // Block polyhedron
    void check_block( const geode::BRep& brep,
        const geode::Block3D& block,
        PressuresPerStep& pressures )
    {
        const auto& mesh = block.mesh();
        const auto& manager = mesh.polyhedron_attribute_manager();
        const geode::AttributeTimeSeries< double > pressure{ manager,
            "pressure" };
        const geode::AttributeTimeSeries< double > geos_porosity{ manager,
            "rockPorosity_referencePorosity" };
        const geode::AttributeTimeSeries< Permeability > geos_permeability{
            manager, "rockPerm_permeability"
        };
        check_series_steps( pressure );
        check_series_steps( geos_porosity );
        check_series_steps( geos_permeability );
        const auto porosity = manager.find_read_only_attribute< double >(
            brep.physical_property_info(
                    geode::PHYSICAL_PROPERTY_NAME::porosity )
                .attribute_id );
        const auto permeability =
            manager.find_read_only_attribute< Permeability >(
                brep.physical_property_info(
                        geode::PHYSICAL_PROPERTY_NAME::permeability )
                    .attribute_id );
        for( const auto step : geode::LIndices{ TIMES } )
        {
            auto& step_pressures = pressures[step];
            for( const auto polyhedron : geode::Range{ mesh.nb_polyhedra() } )
            {
                geode::OpenGeodeIOTimeSeriesException::test(
                    geos_porosity.value( step, polyhedron )
                            == porosity->value( polyhedron )
                        && geos_permeability.value( step, polyhedron )
                               == permeability->value( polyhedron ),
                    "[Test] Wrong GEOS cell matched with polyhedron ",
                    polyhedron, " of Block ", block.name().value_or( "" ) );
                const auto value = pressure.value( step, polyhedron );
                step_pressures.min = std::min( step_pressures.min, value );
                step_pressures.max = std::max( step_pressures.max, value );
                if( value == SOURCE_PRESSURE )
                {
                    step_pressures.nb_source_cells++;
                }
                if( value == SINK_PRESSURE )
                {
                    step_pressures.nb_sink_cells++;
                }
            }
        }
        check_ignored_attributes( mesh );
    }

    void check_brep( const geode::BRep& brep )
    {
        geode::OpenGeodeIOTimeSeriesException::test(
            brep.nb_blocks() == NB_BLOCKS, "[Test] Wrong number of Blocks" );
        PressuresPerStep pressures{};
        geode::index_t nb_cells{ 0 };
        for( const auto& block : brep.blocks() )
        {
            check_block( brep, block, pressures );
            nb_cells += block.mesh().nb_polyhedra();
        }
        geode::OpenGeodeIOTimeSeriesException::test(
            nb_cells == NB_CELLS, "[Test] Wrong number of Block cells" );
        for( const auto step : geode::LIndices{ TIMES } )
        {
            const auto& step_pressures = pressures[step];
            geode::OpenGeodeIOTimeSeriesException::test(
                step_pressures.min == MIN_PRESSURES[step],
                "[Test] Wrong minimum pressure at step ", step );
            geode::OpenGeodeIOTimeSeriesException::test(
                step_pressures.max == SOURCE_PRESSURE,
                "[Test] Wrong maximum pressure at step ", step );
            geode::OpenGeodeIOTimeSeriesException::test(
                step_pressures.nb_source_cells == NB_SOURCE_CELLS,
                "[Test] Wrong number of source cells at step ", step );
            geode::OpenGeodeIOTimeSeriesException::test(
                step_pressures.nb_sink_cells == NB_SINK_CELLS,
                "[Test] Wrong number of sink cells at step ", step );
        }
    }

    void write_collection(
        std::string_view pvd, absl::Span< const std::string > dataset_files )
    {
        std::ofstream file{ std::string{ pvd } };
        file << "<?xml version=\"1.0\"?>\n<VTKFile type=\"Collection\" "
                "version=\"0.1\">\n<Collection>\n";
        for( const auto dataset : geode::Indices{ dataset_files } )
        {
            file << "<DataSet timestep=\"" << dataset << "\" file=\""
                 << dataset_files[dataset] << "\" />\n";
        }
        file << "</Collection>\n</VTKFile>\n";
    }

    void test_import()
    {
        const auto pvd = simulation_file( "picasso_results.pvd" );
        geode::OpenGeodeIOTimeSeriesException::test(
            geode::is_brep_time_series_loadable( pvd ).value() == 1,
            "[Test] Collection should be loadable" );
        const auto files = geode::brep_time_series_additional_files( pvd );
        geode::OpenGeodeIOTimeSeriesException::test(
            !files.has_additional_files(),
            "[Test] Collection should have no missing file" );
        geode::OpenGeodeIOTimeSeriesException::test(
            files.mandatory_files.size() == NB_COLLECTION_FILES,
            "[Test] Collection should list every .vtm and .vtu file" );

        auto brep = load_model();
        geode::load_brep_time_series( brep, pvd );
        check_brep( brep );
        const std::string saved_brep{ "pvd_input/picasso_results.og_brep" };
        geode::save_brep( brep, saved_brep );
        check_brep( geode::load_brep( saved_brep ) );
        geode::load_brep_time_series( brep, pvd );
        check_brep( brep );
    }

    void test_missing_file()
    {
        const std::string pvd{ "pvd_input/missing.pvd" };
        write_collection(
            pvd, { simulation_file( "picasso_results/000000.vtm" ),
                     "missing/step_1.vtm" } );
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

    void test_vtu_datasets()
    {
        const std::string pvd{ "pvd_input/vtu.pvd" };
        write_collection(
            pvd, { simulation_file( "picasso_results/000010/picasso_geos/"
                                    "Level0/Reservoir/rank_0.vtu" ) } );
        geode::OpenGeodeIOTimeSeriesException::test(
            geode::is_brep_time_series_loadable( pvd ).value() == 1,
            "[Test] Collection of .vtu should be loadable" );
        auto brep = load_model();
        geode::load_brep_time_series( brep, pvd );
        for( const auto& block : brep.blocks() )
        {
            const auto nb_steps = block.mesh()
                                      .polyhedron_attribute_manager()
                                      .time_steps( "pressure" )
                                      .size();
            const std::size_t expected_nb_steps = is_reservoir( block ) ? 1 : 0;
            geode::OpenGeodeIOTimeSeriesException::test(
                nb_steps == expected_nb_steps,
                "[Test] Only the reservoir Blocks should have a time step "
                "from the Reservoir .vtu" );
        }
    }

    void test_inconsistent_datasets()
    {
        const std::string vtm{ "pvd_input/reservoir_only.vtm" };
        std::ofstream file{ vtm };
        file << "<?xml version=\"1.0\"?>\n<VTKFile "
                "type=\"vtkMultiBlockDataSet\" version=\"1.0\">\n"
                "<vtkMultiBlockDataSet>\n<DataSet name=\"rank_0\" file=\""
             << simulation_file( "picasso_results/000005/picasso_geos/Level0/"
                                 "Reservoir/rank_0.vtu" )
             << "\" />\n</vtkMultiBlockDataSet>\n</VTKFile>\n";
        file.close();
        const std::string pvd{ "pvd_input/inconsistent.pvd" };
        write_collection(
            pvd, { simulation_file( "picasso_results/000000.vtm" ),
                     "reservoir_only.vtm" } );
        auto brep = load_model();
        bool has_thrown{ false };
        try
        {
            geode::load_brep_time_series( brep, pvd );
        }
        catch( const geode::OpenGeodeException& )
        {
            has_thrown = true;
        }
        geode::OpenGeodeIOTimeSeriesException::test( has_thrown,
            "[Test] Time steps with different numbers of datasets should "
            "not be loaded" );
    }

    void test_unsupported_dataset()
    {
        const std::string pvd{ "pvd_input/unsupported.pvd" };
        write_collection( pvd, { "surface.vtp" } );
        geode::OpenGeodeIOTimeSeriesException::test(
            geode::is_brep_time_series_loadable( pvd ).value() == 0,
            "[Test] Collection of .vtp should not be loadable" );
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
        test_vtu_datasets();
        test_inconsistent_datasets();
        test_unsupported_dataset();
        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
