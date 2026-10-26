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

#include <geode/io/time_series/internal/pvd_input.hpp>

#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <absl/algorithm/container.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_cat.h>

#include <pugixml.hpp>

#include <geode/basic/file.hpp>
#include <geode/basic/filename.hpp>
#include <geode/basic/types.hpp>

#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/core/polyhedral_solid.hpp>
#include <geode/mesh/io/polyhedral_solid_input.hpp>

#include <geode/model/representation/core/brep.hpp>
#include <geode/model/representation/core/external_time_series.hpp>

#include <geode/io/time_series/internal/pvd_attributes_transfer.hpp>
#include <geode/io/time_series/internal/pvd_point_sets.hpp>

namespace
{
    struct PVDDataSet
    {
        double time;
        std::string file;
    };

    // GEOS parallel partitioning bookkeeping arrays, meaningless on the BRep
    constexpr std::array< std::string_view, 2 > GEOS_IGNORED_ATTRIBUTES{
        "localToGlobalMap", "ghostRank"
    };

    std::string lower_extension( std::string_view filename )
    {
        return absl::AsciiStrToLower(
            geode::extension_from_filename( filename ) );
    }

    pugi::xml_node load_vtk_file( pugi::xml_document& document,
        std::string_view filename,
        std::string_view type )
    {
        const auto status =
            document.load_file( geode::to_string( filename ).c_str() );
        geode::OpenGeodeIOTimeSeriesException::check_exception( status, nullptr,
            geode::OpenGeodeException::TYPE::data,
            "[PVDInput] Error while parsing file ", filename, ": ",
            status.description() );
        auto root = document.child( "VTKFile" );
        geode::OpenGeodeIOTimeSeriesException::check_exception(
            geode::to_string( root.attribute( "type" ).value() ) == type,
            nullptr, geode::OpenGeodeException::TYPE::data, "[PVDInput] File ",
            filename, " is not a VTK ", type, " file" );
        return root.child( geode::to_string( type ).c_str() );
    }

    std::string resolve_path( const std::filesystem::path& directory,
        std::string_view referenced_file )
    {
        const std::filesystem::path path{ geode::to_string( referenced_file ) };
        if( path.is_absolute() )
        {
            return path.string();
        }
        return absl::StrCat( directory.string(), referenced_file );
    }

    void add_vtm_block_files( const pugi::xml_node& block,
        const std::filesystem::path& vtm_directory,
        std::vector< std::string >& files )
    {
        for( const auto& child : block.children() )
        {
            const auto name = geode::to_string( child.name() );
            if( name == "Block" )
            {
                add_vtm_block_files( child, vtm_directory, files );
            }
            else if( name == "DataSet" )
            {
                const auto file = child.attribute( "file" ).value();
                if( std::string_view{ file }.empty() )
                {
                    continue;
                }
                files.push_back( resolve_path( vtm_directory, file ) );
            }
        }
    }

    std::vector< PVDDataSet > read_pvd_collection( std::string_view filename )
    {
        pugi::xml_document document;
        const auto collection =
            load_vtk_file( document, filename, "Collection" );
        const auto directory =
            geode::filepath_without_filename( geode::to_string( filename ) );
        std::vector< PVDDataSet > datasets;
        for( const auto& dataset : collection.children( "DataSet" ) )
        {
            const auto time = dataset.attribute( "timestep" );
            geode::OpenGeodeIOTimeSeriesException::check_exception(
                !time.empty(), nullptr, geode::OpenGeodeException::TYPE::data,
                "[read_pvd_collection] DataSet without timestep in ",
                filename );
            const std::string_view referenced_file =
                dataset.attribute( "file" ).value();
            geode::OpenGeodeIOTimeSeriesException::check_exception(
                !referenced_file.empty(), nullptr,
                geode::OpenGeodeException::TYPE::data,
                "[read_pvd_collection] DataSet without file in ", filename );
            geode::OpenGeodeIOTimeSeriesException::check_exception(
                lower_extension( referenced_file ) == "vtm"
                    || lower_extension( referenced_file ) == "vtu",
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[read_pvd_collection] DataSet ", referenced_file, " in ",
                filename, " is neither a .vtm nor a .vtu file" );
            datasets.push_back( { time.as_double(),
                resolve_path( directory, referenced_file ) } );
        }
        absl::c_sort(
            datasets, []( const PVDDataSet& lhs, const PVDDataSet& rhs ) {
                return lhs.time < rhs.time;
            } );
        return datasets;
    }

    // GEOS writes one .vtu per region and per MPI rank in each .vtm
    std::vector< std::string > dataset_vtu_files( std::string_view filename )
    {
        if( lower_extension( filename ) == "vtu" )
        {
            return { geode::to_string( filename ) };
        }
        pugi::xml_document document;
        const auto multiblock =
            load_vtk_file( document, filename, "vtkMultiBlockDataSet" );
        std::vector< std::string > files;
        add_vtm_block_files( multiblock,
            geode::filepath_without_filename( geode::to_string( filename ) ),
            files );
        geode::OpenGeodeIOTimeSeriesException::check_exception( !files.empty(),
            nullptr, geode::OpenGeodeException::TYPE::data,
            "[dataset_vtu_files] ", filename, " lists no dataset" );
        for( const auto& file : files )
        {
            geode::OpenGeodeIOTimeSeriesException::check_exception(
                lower_extension( file ) == "vtu", nullptr,
                geode::OpenGeodeException::TYPE::data, "[dataset_vtu_files] ",
                file, " is not a .vtu dataset" );
        }
        return files;
    }

    std::vector< std::string > collection_files( std::string_view filename )
    {
        std::vector< std::string > files;
        for( const auto& dataset : read_pvd_collection( filename ) )
        {
            files.push_back( dataset.file );
            if( lower_extension( dataset.file ) == "vtm"
                && geode::file_exists( dataset.file ) )
            {
                for( auto& vtu : dataset_vtu_files( dataset.file ) )
                {
                    files.push_back( std::move( vtu ) );
                }
            }
        }
        return files;
    }

    geode::ExternalTimeSeries read_time_series( std::string_view filename )
    {
        geode::internal::PVDAttributesTransfer transfer{
            GEOS_IGNORED_ATTRIBUTES
        };
        std::optional< geode::internal::PVDPointSets > point_sets;
        for( const auto& dataset : read_pvd_collection( filename ) )
        {
            std::vector< std::unique_ptr< geode::SolidMesh3D > > meshes;
            for( const auto& vtu : dataset_vtu_files( dataset.file ) )
            {
                meshes.push_back( geode::load_polyhedral_solid< 3 >( vtu ) );
            }
            if( !point_sets )
            {
                point_sets = geode::internal::create_pvd_point_sets( meshes );
            }
            const auto nb_datasets = point_sets->mappings.size();
            geode::OpenGeodeIOTimeSeriesException::check_exception(
                meshes.size() == nb_datasets, nullptr,
                geode::OpenGeodeException::TYPE::data, "[PVDInput] ",
                dataset.file, " lists ", meshes.size(),
                " datasets whereas the first time step lists ", nb_datasets );
            transfer.write_step( dataset.time, meshes, point_sets.value() );
        }
        geode::ExternalTimeSeries time_series;
        if( point_sets )
        {
            time_series.set_unique_vertices(
                std::move( point_sets->unique_vertices ) );
            time_series.set_cell_barycenters(
                std::move( point_sets->cell_barycenters ) );
        }
        return time_series;
    }
} // namespace

namespace geode::internal
{
    AdditionalFiles PVDBRepTimeSeriesInput::additional_files() const
    {
        AdditionalFiles files;
        for( auto& file : collection_files( this->filename() ) )
        {
            const auto is_missing = !file_exists( file );
            files.mandatory_files.emplace_back( std::move( file ), is_missing );
        }
        return files;
    }

    Percentage PVDBRepTimeSeriesInput::is_loadable() const
    {
        try
        {
            const auto files = collection_files( this->filename() );
            if( files.empty() )
            {
                return Percentage{ 0 };
            }
            const auto nb_existing =
                absl::c_count_if( files, []( const std::string& file ) {
                    return file_exists( file );
                } );
            return Percentage{ static_cast< double >( nb_existing )
                               / static_cast< double >( files.size() ) };
        }
        catch( ... )
        {
            return Percentage{ 0 };
        }
    }

    void PVDBRepTimeSeriesInput::read( BRep& brep )
    {
        read_time_series( this->filename() ).apply_to( brep );
    }
} // namespace geode::internal
