/*
 * Copyright (c) 2019 - 2024 Geode-solutions
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

#include <geode/mesh/builder/graph_builder.hpp>
#include <geode/mesh/core/graph.hpp>
#include <geode/mesh/io/graph_output.hpp>

#include <geode/io/mesh/common.hpp>

int main()
{
    try
    {
        geode::IOMeshLibrary::initialize();
        // Build graph
        auto graph = geode::Graph::create();
        auto builder = geode::GraphBuilder::create( *graph );
        builder->create_vertices( 10 );
        builder->create_edge( 0, 1 );
        builder->create_edge( 1, 2 );
        builder->create_edge( 2, 3 );
        builder->create_edge( 3, 4 );
        builder->create_edge( 4, 5 );
        builder->create_edge( 5, 6 );
        builder->create_edge( 6, 7 );
        builder->create_edge( 7, 8 );
        builder->create_edge( 8, 9 );
        builder->create_edge( 9, 0 );
        builder->create_edge( 0, 2 );
        builder->create_edge( 2, 4 );
        builder->create_edge( 4, 6 );
        builder->create_edge( 6, 8 );
        builder->create_edge( 8, 0 );
        builder->create_edge( 0, 3 );
        builder->create_edge( 3, 5 );
        builder->create_edge( 5, 7 );
        builder->create_edge( 7, 9 );
        builder->create_edge( 9, 1 );
        builder->create_edge( 1, 3 );

        // Save file
        geode::save_graph( *graph, "graph.gexf" );

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
