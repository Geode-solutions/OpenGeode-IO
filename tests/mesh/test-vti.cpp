/*
 * Copyright (c) 2019 - 2022 Geode-solutions
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
#include <geode/basic/attribute_manager.h>
#include <geode/basic/logger.h>

#include <geode/geometry/point.h>

#include <geode/mesh/core/regular_grid.h>
#include <geode/mesh/io/regular_grid_output.h>

#include <geode/io/mesh/detail/common.h>

int main()
{
    try
    {
        geode::detail::initialize_mesh_io();

        geode::RegularGrid3D grid{ { { 1, 2, 3 } }, { 10, 20, 30 }, 1 };
        auto att = grid.cell_attribute_manager()
                       .find_or_create_attribute< geode::VariableAttribute,
                           geode::index_t >( "id", geode::NO_ID );
        for( const auto c : geode::Range{ grid.nb_cells() } )
        {
            att->set_value( c, c );
        }
        auto att_vertex =
            grid.vertex_attribute_manager()
                .find_or_create_attribute< geode::VariableAttribute,
                    geode::index_t >( "id_vertex", geode::NO_ID );
        for( const auto c : geode::Range{ grid.nb_vertices() } )
        {
            att_vertex->set_value( c, c );
        }

        geode::save_regular_grid( grid, "test.vti" );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
