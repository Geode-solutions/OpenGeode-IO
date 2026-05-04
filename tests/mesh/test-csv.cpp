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
#include <geode/basic/input.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/range.hpp>

#include <geode/geometry/point.hpp>

#include <geode/io/mesh/common.hpp>
#include <geode/io/mesh/csv_input_helpers.hpp>
#include <geode/mesh/core/point_set.hpp>
#include <geode/mesh/io/point_set_input.hpp>
#include <geode/mesh/io/point_set_output.hpp>

void test_csv_input()
{
    const auto filepath =
        absl::StrCat( geode::DATA_PATH, "geological_pointset3d.csv" );
    const auto additional_files =
        geode::point_set_additional_files< 3 >( filepath );
    OPENGEODE_EXCEPTION( additional_files.has_additional_files(),
        "[TEST: CSV input], Additional files should be present" );
    auto point_set = geode::load_point_set< 3 >( filepath );
    OPENGEODE_EXCEPTION( point_set->nb_vertices() == 500,
        "[TEST: CSV input], "
        "The point set should have 500 vertices found",
        point_set->nb_vertices() );
    geode::save_point_set( *point_set, "result.og_pts3d" );
}

void test_csv_input_with_missing_json()
{
    const auto filepath =
        absl::StrCat( geode::DATA_PATH, "other_geological_pointset3d.csv" );
    const auto additional_files =
        geode::point_set_additional_files< 3 >( filepath );
    OPENGEODE_EXCEPTION( !additional_files.has_additional_files(),
        "[TEST: CSV input], Additional files should be missing because of a "
        "missing keyword" );
}

void test_csv_input_with_missing_keyword()
{
    const auto filepath =
        absl::StrCat( geode::DATA_PATH, "mising_keyword.csv" );
    const auto additional_files =
        geode::point_set_additional_files< 3 >( filepath );
    OPENGEODE_EXCEPTION( !additional_files.has_additional_files(),
        "[TEST: CSV input], Additional files should be missing because of a "
        "missing keyword" );
}

int main()
{
    try
    {
        geode::IOMeshLibrary::initialize();
        geode::Logger::set_level( geode::Logger::LEVEL::trace );
        test_csv_input();
        test_csv_input_with_missing_json();
        test_csv_input_with_missing_keyword();
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}