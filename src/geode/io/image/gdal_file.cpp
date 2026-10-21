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

#include <geode/io/image/detail/gdal_file.hpp>

#include <filesystem>

#include <gdal_priv.h>

#include <geode/basic/logger.hpp>
#include <geode/basic/pimpl_impl.hpp>

#include <geode/geometry/coordinate_system.hpp>

namespace
{
    void CPL_STDCALL log_gdal_warnings(
        CPLErr type, CPLErrorNum /*unused*/, const char* message )
    {
        if( type == CE_Warning )
        {
            geode::Logger::warning( "[GDAL] ", message );
        }
    }
} // namespace

namespace geode
{
    namespace detail
    {
        GDALErrorScope::GDALErrorScope()
        {
            CPLPushErrorHandler( log_gdal_warnings );
            CPLErrorReset();
        }

        GDALErrorScope::~GDALErrorScope()
        {
            CPLPopErrorHandler();
        }

        std::string GDALErrorScope::last_error() const
        {
            return CPLGetLastErrorMsg();
        }

        class GDALFile::Impl
        {
        public:
            Impl( std::string_view filename,
                absl::Span< const char* const > allowed_drivers )
            {
                const GDALErrorScope errors;
                dataset_.reset( open( filename, allowed_drivers ) );
                OpenGeodeIOImageException::check_exception( dataset_ != nullptr,
                    nullptr, OpenGeodeException::TYPE::data,
                    "[GDALFile] Failed to open file ", filename, ": ",
                    errors.last_error() );
            }

            GDALDataset& dataset()
            {
                return *dataset_;
            }

            CoordinateSystem2D read_coordinate_system()
            {
                std::array< double, 6 > geo_transform;
                const auto status =
                    dataset_->GetGeoTransform( geo_transform.data() );
                OpenGeodeIOImageException::check_exception( status == CE_None,
                    nullptr, OpenGeodeException::TYPE::data,
                    "Failed to read geotransform from GDALDataset" );
                Point2D origin;
                Vector2D x_direction;
                Vector2D y_direction;
                for( const auto axis : geode::LRange{ 2 } )
                {
                    origin.set_value( axis, geo_transform[3 * axis] );
                    x_direction.set_value( axis, geo_transform[3 * axis + 1] );
                    y_direction.set_value( axis, geo_transform[3 * axis + 2] );
                }
                return { { x_direction, y_direction }, origin };
            }

            bool is_coordinate_system_loadable()
            {
                std::array< double, 6 > geo_transform;
                const auto status =
                    dataset_->GetGeoTransform( geo_transform.data() );
                return status == CE_None;
            }

            std::vector< std::string > associated_files()
            {
                char** papszFileList = dataset_->GetFileList();
                if( !papszFileList )
                {
                    return {};
                }
                // The list starts with the main file, which is not associated
                const std::filesystem::path main_file{
                    dataset_->GetDescription()
                };
                std::vector< std::string > files;
                for( int i = 0; papszFileList[i] != nullptr; i++ )
                {
                    std::error_code error;
                    if( papszFileList[i] == main_file
                        || std::filesystem::equivalent(
                            papszFileList[i], main_file, error ) )
                    {
                        continue;
                    }
                    files.emplace_back( papszFileList[i] );
                }
                CSLDestroy( papszFileList );
                return files;
            }

        private:
            static GDALDataset* open( std::string_view filename,
                absl::Span< const char* const > allowed_drivers )
            {
                if( allowed_drivers.empty() )
                {
                    return GDALDataset::Open(
                        geode::to_string( filename ).c_str(),
                        GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR );
                }
                std::vector< const char* > drivers{ allowed_drivers.begin(),
                    allowed_drivers.end() };
                drivers.push_back( nullptr );
                return GDALDataset::Open( geode::to_string( filename ).c_str(),
                    GDAL_OF_READONLY | GDAL_OF_VERBOSE_ERROR, drivers.data() );
            }

        private:
            GDALDatasetUniquePtr dataset_;
        };

        GDALFile::GDALFile( std::string_view filename,
            absl::Span< const char* const > allowed_drivers )
            : impl_{ filename, allowed_drivers }
        {
        }

        GDALFile::~GDALFile() = default;

        GDALDataset& GDALFile::dataset()
        {
            return impl_->dataset();
        }

        std::vector< std::string > GDALFile::associated_files()
        {
            return impl_->associated_files();
        }

        CoordinateSystem2D GDALFile::read_coordinate_system()
        {
            return impl_->read_coordinate_system();
        }

        bool GDALFile::is_coordinate_system_loadable()
        {
            return impl_->is_coordinate_system_loadable();
        }
    } // namespace detail
} // namespace geode