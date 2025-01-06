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

#include <geode/io/mesh/internal/gexf_output.hpp>

#include <string>
#include <vector>

#include <pugixml.hpp>

#include <geode/mesh/core/graph.hpp>

namespace
{
    pugi::xml_node write_header( pugi::xml_document &doc )
    {
        auto gexf_node = doc.append_child( "gexf" );
        gexf_node.append_attribute( "version" ).set_value( "1.1" );
        auto graph_node = gexf_node.append_child( "graph" );
        graph_node.append_attribute( "defaultedgetype" )
            .set_value( "undirected" );
        graph_node.append_attribute( "idtype" ).set_value( "string" );
        graph_node.append_attribute( "type" ).set_value( "static" );
        return graph_node;
    }

    void write_nodes( pugi::xml_node &graph_node, const geode::Graph &graph )
    {
        auto child_node = graph_node.append_child( "nodes" );
        child_node.append_attribute( "count" ).set_value( graph.nb_vertices() );
        for( const auto vertex_id : geode::Range{ graph.nb_vertices() } )
        {
            auto node = child_node.append_child( "node" );
            node.append_attribute( "id" ).set_value( vertex_id );
        }
    }

    void write_edges( pugi::xml_node &graph_node, const geode::Graph &graph )
    {
        auto child_node = graph_node.append_child( "edges" );
        child_node.append_attribute( "count" ).set_value( graph.nb_edges() );
        for( const auto edge_id : geode::Range{ graph.nb_edges() } )
        {
            auto edge = child_node.append_child( "edge" );
            edge.append_attribute( "id" ).set_value( edge_id );
            const auto edge_vertices = graph.edge_vertices( edge_id );
            edge.append_attribute( "source" ).set_value( edge_vertices[0] );
            edge.append_attribute( "target" ).set_value( edge_vertices[1] );
        }
    }
} // namespace

namespace geode
{
    namespace internal
    {
        std::vector< std::string > GEXFOutput::write( const Graph &graph ) const
        {
            pugi::xml_document doc;
            auto graph_node = write_header( doc );
            write_nodes( graph_node, graph );
            write_edges( graph_node, graph );
            doc.save_file( to_string( filename() ).c_str(),
                PUGIXML_TEXT( "    " ), pugi::format_indent_attributes );
            return { to_string( filename() ) };
        }
    } // namespace internal
} // namespace geode
