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

#include <geode/tests_config.h>

#include <geode/basic/assert.h>
#include <geode/basic/logger.h>
#include <geode/basic/range.h>

#include <geode/mesh/core/edged_curve.h>
#include <geode/mesh/core/point_set.h>
#include <geode/mesh/core/polygonal_surface.h>

#include <geode/model/mixin/core/corner.h>
#include <geode/model/mixin/core/line.h>
#include <geode/model/mixin/core/surface.h>
#include <geode/model/representation/core/section.h>
#include <geode/model/representation/io/section_input.h>
#include <geode/model/representation/io/section_output.h>

#include <geode/io/model/common.h>

geode::index_t nb_closed_lines( const geode::Section& section )
{
    geode::index_t nb{ 0 };
    for( const auto& line : section.lines() )
    {
        if( section.is_closed( line ) )
        {
            nb++;
        }
    }
    return nb;
}

void test_section( const geode::Section& section )
{
    OPENGEODE_EXCEPTION(
        section.nb_corners() == 31, "[Test] Number of corners is not correct" );
    OPENGEODE_EXCEPTION(
        section.nb_lines() == 31, "[Test] Number of lines is not correct" );
    OPENGEODE_EXCEPTION( nb_closed_lines( section ) == 27,
        "[Test] Number of closed lines is not correct" );
    OPENGEODE_EXCEPTION( section.nb_surfaces() == 0,
        "[Test] Number of surfaces is not correct" );
    for( const auto uv : geode::Range{ section.nb_unique_vertices() } )
    {
        const auto nb_cmv = section.component_mesh_vertices( uv ).size();
        OPENGEODE_EXCEPTION( nb_cmv == 1 || nb_cmv == 3,
            "[Test] Wrong number of mesh component "
            "vertices for one unique vertex" );
    }
}

int main()
{
    try
    {
        geode::IOModelLibrary::initialize();

        // Load file
        auto section = geode::load_section(
            absl::StrCat( geode::data_path, "/logo.svg" ) );
        test_section( section );

        // Save and reload
        const auto filename =
            absl::StrCat( "logo.", section.native_extension() );
        geode::save_section( section, filename );
        auto reloaded_section = geode::load_section( filename );
        test_section( reloaded_section );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
