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

#include <geode/io/image/detail/gdal_file.hpp>

#include <gdal_priv.h>

#include <geode/basic/pimpl_impl.hpp>

#include <geode/geometry/coordinate_system.hpp>

namespace geode
{
    namespace detail
    {
        class GDALFile::Impl
        {
        public:
            Impl( std::string_view filename )
                : dataset_{ GDALDataset::Open(
                      geode::to_string( filename ).c_str(), GDAL_OF_READONLY ) }
            {
                OPENGEODE_EXCEPTION(
                    dataset_, "[GDALFile] Failed to open file ", filename );
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
                OPENGEODE_EXCEPTION( status == CE_None,
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
                return { origin, { x_direction, y_direction } };
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
                std::vector< std::string > files;
                for( int i = 0; papszFileList[i] != nullptr; i++ )
                {
                    files.emplace_back( papszFileList[i] );
                }
                CSLDestroy( papszFileList );
                return files;
            }

        private:
            GDALDatasetUniquePtr dataset_;
        };

        GDALFile::GDALFile( std::string_view filename ) : impl_{ filename } {}

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