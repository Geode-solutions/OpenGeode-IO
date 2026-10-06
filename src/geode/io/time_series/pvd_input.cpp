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

#include <filesystem>
#include <optional>
#include <vector>

#include <absl/algorithm/container.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_cat.h>

#include <pugixml.hpp>

#include <geode/basic/file.hpp>
#include <geode/basic/filename.hpp>
#include <geode/basic/types.hpp>

#include <geode/mesh/core/polyhedral_solid.hpp>
#include <geode/mesh/io/polyhedral_solid_input.hpp>

#include <geode/model/representation/core/brep.hpp>

#include <geode/io/time_series/internal/brep_blocks_matcher.hpp>
#include <geode/io/time_series/internal/brep_time_attributes_transfer.hpp>

namespace
{
    struct PVDDataSet
    {
        double time;
        std::string file;
    };

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

    std::string resolve_path(
        std::string_view referencing_file, std::string_view referenced_file )
    {
        const std::filesystem::path path{ geode::to_string( referenced_file ) };
        if( path.is_absolute() )
        {
            return path.string();
        }
        return absl::StrCat( geode::filepath_without_filename(
                                 geode::to_string( referencing_file ) )
                                 .string(),
            referenced_file );
    }

    void add_vtm_block_files( const pugi::xml_node& block,
        std::string_view vtm_filename,
        std::vector< std::string >& files )
    {
        for( const auto& child : block.children() )
        {
            const auto name = geode::to_string( child.name() );
            if( name == "Block" )
            {
                add_vtm_block_files( child, vtm_filename, files );
            }
            else if( name == "DataSet" )
            {
                const auto file = child.attribute( "file" ).value();
                if( std::string_view{ file }.empty() )
                {
                    continue;
                }
                files.push_back( resolve_path( vtm_filename, file ) );
            }
        }
    }

    std::vector< PVDDataSet > read_pvd_collection( std::string_view filename )
    {
        pugi::xml_document document;
        const auto collection =
            load_vtk_file( document, filename, "Collection" );
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
            datasets.push_back( { time.as_double(),
                resolve_path( filename, referenced_file ) } );
        }
        absl::c_sort(
            datasets, []( const PVDDataSet& lhs, const PVDDataSet& rhs ) {
                return lhs.time < rhs.time;
            } );
        return datasets;
    }

    std::string dataset_vtu_file( std::string_view filename )
    {
        const auto extension =
            absl::AsciiStrToLower( geode::extension_from_filename( filename ) );
        geode::OpenGeodeIOTimeSeriesException::check_exception(
            extension == "vtm", nullptr, geode::OpenGeodeException::TYPE::data,
            "[dataset_vtu_file] ", filename, " is not a vtm dataset" );
        pugi::xml_document document;
        const auto multiblock =
            load_vtk_file( document, filename, "vtkMultiBlockDataSet" );
        std::vector< std::string > files;
        add_vtm_block_files( multiblock, filename, files );
        geode::OpenGeodeIOTimeSeriesException::check_exception(
            files.size() == 1, nullptr, geode::OpenGeodeException::TYPE::data,
            "[dataset_vtu_file] ", filename, " lists ", files.size(),
            " datasets: only one .vtu per time step is supported" );
        geode::OpenGeodeIOTimeSeriesException::check_exception(
            absl::AsciiStrToLower(
                geode::extension_from_filename( files.front() ) )
                == "vtu",
            nullptr, geode::OpenGeodeException::TYPE::data,
            "[dataset_vtu_file] ", files.front(), " is not a .vtu dataset" );
        return files.front();
    }

    bool is_vtm( std::string_view filename )
    {
        return absl::AsciiStrToLower(
                   geode::extension_from_filename( filename ) )
               == "vtm";
    }

    std::vector< std::string > collection_files( std::string_view filename )
    {
        std::vector< std::string > files;
        for( const auto& dataset : read_pvd_collection( filename ) )
        {
            files.push_back( dataset.file );
            if( is_vtm( dataset.file ) && geode::file_exists( dataset.file ) )
            {
                files.push_back( dataset_vtu_file( dataset.file ) );
            }
        }
        return files;
    }
} // namespace

namespace geode
{
    namespace internal
    {
        AdditionalFiles PVDBRepTimeSeriesInput::additional_files() const
        {
            AdditionalFiles files;
            for( auto& file : collection_files( this->filename() ) )
            {
                const auto is_missing = !file_exists( file );
                files.mandatory_files.emplace_back(
                    std::move( file ), is_missing );
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
                                   / files.size() };
            }
            catch( ... )
            {
                return Percentage{ 0 };
            }
        }

        void PVDBRepTimeSeriesInput::read( BRep& brep )
        {
            const BRepBlocksMatcher matcher{ brep };
            BRepTimeAttributesTransfer transfer{ brep };
            std::optional< ModelToSolidMappings > mappings;
            for( const auto& dataset : read_pvd_collection( this->filename() ) )
            {
                const auto vtu = dataset_vtu_file( dataset.file ); //
                const auto mesh = load_polyhedral_solid< 3 >( vtu );
                if( !mappings )
                {
                    mappings = matcher.mappings( *mesh );
                }
                transfer.write_step( dataset.time, *mesh, mappings.value() );
            }
        }
    } // namespace internal
} // namespace geode
