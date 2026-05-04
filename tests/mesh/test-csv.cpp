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

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/geometry/point.hpp>

#include <geode/io/mesh/common.hpp>
#include <geode/io/mesh/csv_input_helpers.hpp>
#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/io/point_set_output.hpp>

int main()
{
    try
    {
        geode::IOMeshLibrary::initialize();
        geode::Logger::set_level( geode::Logger::LEVEL::trace );
        geode::CsvInputHelpers reader{ absl::StrCat(
            geode::DATA_PATH, "geological_pointset3d.csv" ) };
        reader.set_separator( ';' );
        reader.set_x_column( 1 );
        reader.set_y_column( 2 );
        reader.set_z_column( 3 );
        const auto point_set = reader.create_point_set();
        for( const auto vertex : geode::Range{ point_set->nb_vertices() } )
        {
            SDEBUG( point_set->point( vertex ) );
        }
        geode::save_point_set( *point_set, "test_points.og_pts3d" );
        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}