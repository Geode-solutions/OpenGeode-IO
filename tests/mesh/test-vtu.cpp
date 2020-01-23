/*
 * Copyright (c) 2019 - 2020 Geode-solutions
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
#include <geode/basic/attribute.h>
#include <geode/basic/attribute_manager.h>
#include <geode/basic/logger.h>

#include <geode/geometry/point.h>

#include <geode/mesh/builder/polyhedral_solid_builder.h>
#include <geode/mesh/core/polyhedral_solid.h>
#include <geode/mesh/detail/common.h>
#include <geode/mesh/detail/vtu_output.h>

void initialize_solid( geode::PolyhedralSolidBuilder3D& builder )
{
    builder.create_point( { { 0., 0., 0. } } );
    builder.create_point( { { 1., 0.5, 0. } } );
    builder.create_point( { { 2., -1., 0. } } );
    builder.create_point( { { 2., -2., 0. } } );
    builder.create_point( { { 1., -2., 0. } } );
    builder.create_point( { { 0., -1., 2. } } );
    builder.create_point( { { 2., -4., 0. } } );
    builder.create_point( { { 1., -4., 0. } } );
    builder.create_point( { { 3., -3., 3. } } );
    builder.create_polyhedron(
        { 0, 1, 2, 3, 4, 5 }, { { 0, 1, 2, 3, 4 }, { 0, 1, 5 }, { 1, 2, 5 },
                                  { 2, 3, 5 }, { 3, 4, 5 }, { 4, 0, 5 } } );
    builder.create_polyhedron(
        { 3, 4, 5, 6, 7 }, { { 0, 1, 4, 3 }, { 0, 1, 2 }, { 1, 4, 2 },
                               { 4, 3, 2 }, { 3, 0, 2 } } );
    builder.create_polyhedron( { 5, 6, 7, 8 },
        { { 0, 1, 2 }, { 1, 2, 3 }, { 2, 3, 0 }, { 3, 0, 1 } } );
}

void initialize_solid_attributes( geode::PolyhedralSolid3D& solid )
{
    auto attr = solid.polyhedron_attribute_manager()
                    .find_or_create_attribute< geode::VariableAttribute,
                        geode::index_t >( "index_t", 0 );
    for( const auto p : geode::Range{ solid.nb_polyhedra() } )
    {
        attr->set_value( p, 2 * p );
    }
    auto attr2 =
        solid.vertex_attribute_manager()
            .find_or_create_attribute< geode::VariableAttribute, double >(
                "double", 0 );
    for( const auto v : geode::Range{ solid.nb_vertices() } )
    {
        attr2->set_value( v, 5.0 - 2.0 * v );
    }
}

int main()
{
    using namespace geode;

    try
    {
        initialize_mesh_io();
        auto solid = PolyhedralSolid3D::create();
        auto builder = PolyhedralSolidBuilder3D::create( *solid );
        initialize_solid( *builder );
        initialize_solid_attributes( *solid );

        // Save file
        const std::string output_file{ "solid." + VTUOutput::extension() };
        save_polyhedral_solid( *solid, output_file );

        Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode_lippincott();
    }
}
