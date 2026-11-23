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

#include <array>
#include <fstream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <absl/strings/match.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>
#include <geode/basic/logger_client.hpp>
#include <geode/basic/logger_manager.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/polygonal_surface_builder.hpp>
#include <geode/mesh/builder/triangulated_surface_builder.hpp>
#include <geode/mesh/core/polygonal_surface.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>
#include <geode/mesh/io/polygonal_surface_input.hpp>
#include <geode/mesh/io/polygonal_surface_output.hpp>
#include <geode/mesh/io/triangulated_surface_input.hpp>
#include <geode/mesh/io/triangulated_surface_output.hpp>

#include <geode/io/mesh/common.hpp>

namespace
{
    /* Records the warnings to check what the reader reports */
    class WarningRecorder : public geode::LoggerClient
    {
    public:
        static void start()
        {
            static const auto registered = [] {
                geode::LoggerManager::register_client(
                    std::make_unique< WarningRecorder >() );
                return true;
            }();
            geode_unused( registered );
            warnings().clear();
        }

        [[nodiscard]] static bool has_warning( std::string_view text )
        {
            for( const auto& warning : warnings() )
            {
                if( absl::StrContains( warning, text ) )
                {
                    return true;
                }
            }
            return false;
        }

        void trace( const std::string& /*unused*/ ) override {}
        void debug( const std::string& /*unused*/ ) override {}
        void info( const std::string& /*unused*/ ) override {}
        void warning( const std::string& message ) override
        {
            warnings().push_back( message );
        }
        void error( const std::string& /*unused*/ ) override {}
        void critical( const std::string& /*unused*/ ) override {}

    private:
        static std::vector< std::string >& warnings()
        {
            static std::vector< std::string > recorded;
            return recorded;
        }
    };

    void write_file( const std::string& filename, std::string_view content )
    {
        std::ofstream file{ filename, std::ios::binary };
        file << content;
    }

    void check_surface( const geode::PolygonalSurface3D& surface,
        std::string_view name,
        geode::index_t nb_vertices,
        geode::index_t nb_polygons )
    {
        geode::OpenGeodeIOMeshException::test(
            surface.name().value_or( "" ) == name, "Wrong surface name: \"",
            surface.name().value_or( "" ), "\" instead of \"", name, "\"" );
        geode::OpenGeodeIOMeshException::test(
            surface.nb_vertices() == nb_vertices,
            "Wrong number of vertices in ", name, ": ", surface.nb_vertices() );
        geode::OpenGeodeIOMeshException::test(
            surface.nb_polygons() == nb_polygons,
            "Wrong number of polygons in ", name, ": ", surface.nb_polygons() );
    }

    void check_adjacency( const geode::PolygonalSurface3D& surface,
        geode::PolygonEdge edge,
        std::optional< geode::index_t > adjacent )
    {
        geode::OpenGeodeIOMeshException::test(
            surface.polygon_adjacent( edge ) == adjacent, "Wrong adjacency of ",
            surface.name().value_or( "" ), " polygon ", edge.polygon_id,
            " edge ", edge.edge_id );
    }

    void expect_data_error(
        std::string_view filename, std::string_view expected_message )
    {
        try
        {
            const auto surfaces =
                geode::load_polygonal_surface< 3 >( filename );
        }
        catch( const geode::OpenGeodeException& exception )
        {
            geode::OpenGeodeIOMeshException::test(
                exception.type() == geode::OpenGeodeException::TYPE::data
                    && absl::StrContains( exception.what(), expected_message ),
                "Wrong error loading ", filename, ": ", exception.what() );
            return;
        }
        throw geode::OpenGeodeIOMeshException{ nullptr,
            geode::OpenGeodeException::TYPE::internal, "Loading ", filename,
            " should raise a data error" };
    }

    void test_polyface_surfaces()
    {
        const auto filename =
            absl::StrCat( geode::DATA_PATH, "polyface_surfaces.dxf" );
        geode::OpenGeodeIOMeshException::test(
            geode::is_polygonal_surface_loadable< 3 >( filename ).value() == 1,
            "polyface_surfaces.dxf should be loadable" );
        const auto surfaces = geode::load_polygonal_surface< 3 >( filename );
        geode::OpenGeodeIOMeshException::test( surfaces.size() == 3,
            "Wrong number of polyface surfaces: ", surfaces.size() );

        const auto& galerie = *surfaces[0];
        check_surface( galerie, "Galerie A", 6, 3 );
        const std::array< std::vector< geode::index_t >, 3 > expected{ {
            { 0, 1, 3 },
            { 1, 2, 3 },
            { 1, 4, 5, 2 },
        } };
        for( const auto p : geode::LIndices{ expected } )
        {
            geode::OpenGeodeIOMeshException::test(
                galerie.nb_polygon_vertices( p ) == expected[p].size(),
                "Wrong size of polygon ", p );
            for( const auto v : geode::LIndices{ expected[p] } )
            {
                geode::OpenGeodeIOMeshException::test(
                    galerie.polygon_vertex( { p, v } ) == expected[p][v],
                    "Wrong vertex ", v, " of polygon ", p );
            }
        }
        geode::OpenGeodeIOMeshException::test(
            galerie.point( 5 ).inexact_equal( geode::Point3D{ { 2, 1, 0 } } ),
            "Wrong position of vertex 5" );
        check_adjacency( galerie, { 0, 0 }, std::nullopt );
        check_adjacency( galerie, { 0, 1 }, 1 );
        check_adjacency( galerie, { 1, 2 }, 0 );
        check_adjacency( galerie, { 1, 0 }, 2 );
        check_adjacency( galerie, { 2, 3 }, 1 );

        const auto& baie = *surfaces[1];
        check_surface( baie,
            "Baie d\xC3\xA9"
            "cantation 1",
            4, 2 );
        check_adjacency( baie, { 0, 2 }, 1 );
        check_adjacency( baie, { 1, 0 }, 0 );
        geode::OpenGeodeIOMeshException::test(
            baie.point( 0 ).inexact_equal( geode::Point3D{ { 0, 0, 10 } } ),
            "Wrong position of vertex 0 in second surface" );

        check_surface( *surfaces[2], "OTHER_3C", 3, 1 );
    }

    void test_3d_faces()
    {
        // Expected values computed by a script independent of the reader
        const std::array<
            std::tuple< std::string_view, geode::index_t, geode::index_t >, 14 >
            expected{ {
                { "S00L020001", 5523, 10236 },
                { "B003001003", 7597, 14814 },
                { "S00L030003", 2535, 4715 },
                { "S00L040003", 3329, 6291 },
                { "S00L050003", 3531, 6687 },
                { "S00L020003", 1697, 3082 },
                { "S00L060003", 3389, 6284 },
                { "S00L070003", 1883, 3333 },
                { "S00L030001", 5857, 11102 },
                { "S00L040001", 5052, 9644 },
                { "S00L050001", 4441, 8419 },
                { "B004001002", 3934, 7690 },
                { "S00L060001", 2548, 4705 },
                { "S00L070001", 568, 964 },
            } };
        const auto filename = absl::StrCat( geode::DATA_PATH, "3D_faces.dxf" );
        const auto surfaces = geode::load_polygonal_surface< 3 >( filename );
        geode::OpenGeodeIOMeshException::test(
            surfaces.size() == expected.size(),
            "Wrong number of 3DFACE surfaces: ", surfaces.size() );
        geode::index_t nb_polygons{ 0 };
        for( const auto s : geode::LIndices{ expected } )
        {
            const auto& [name, nb_vertices, nb_layer_polygons] = expected[s];
            check_surface( *surfaces[s], name, nb_vertices, nb_layer_polygons );
            nb_polygons += surfaces[s]->nb_polygons();
        }
        geode::OpenGeodeIOMeshException::test( nb_polygons == 97966,
            "Wrong total number of polygons: ", nb_polygons );
        for( const auto p : geode::Range{ surfaces[0]->nb_polygons() } )
        {
            geode::OpenGeodeIOMeshException::test(
                surfaces[0]->polygon_adjacent( { p, 0 } )
                    || surfaces[0]->polygon_adjacent( { p, 1 } )
                    || surfaces[0]->polygon_adjacent( { p, 2 } ),
                "3DFACE polygons should be connected" );
        }

        const auto output_file_og =
            absl::StrCat( "dxf.", surfaces[0]->native_extension() );
        geode::save_polygonal_surface( *surfaces[0], output_file_og );
    }

    void test_face_and_line()
    {
        const auto filename =
            absl::StrCat( geode::DATA_PATH, "face_and_line.dxf" );
        auto face_and_line = geode::load_one(
            geode::load_polygonal_surface< 3 >( filename ), filename );
        check_surface( *face_and_line, "0", 3, 1 );
    }

    void test_binary_dxf()
    {
        const std::string filename{ "binary.dxf" };
        {
            std::ofstream file{ filename, std::ios::binary };
            file << "AutoCAD Binary DXF\r\n";
            file.put( '\x1a' );
            file.put( '\0' );
        }
        geode::OpenGeodeIOMeshException::test(
            geode::is_polygonal_surface_loadable< 3 >( filename ).value() == 0,
            "A binary DXF should not be loadable" );
        expect_data_error( filename, "Binary DXF" );
    }

    void test_trailing_content()
    {
        // Content after the ENTITIES section or after EOF is not read
        const std::string filename{ "trailing_content.dxf" };
        {
            std::ofstream file{ filename, std::ios::binary };
            file << "  0\nSECTION\n  2\nENTITIES\n  0\n3DFACE\n  8\nLAYER\n"
                    " 10\n0.0\n 20\n0.0\n 30\n0.0\n 11\n1.0\n 21\n0.0\n"
                    " 31\n0.0\n 12\n0.0\n 22\n1.0\n 32\n0.0\n 13\n0.0\n"
                    " 23\n1.0\n 33\n0.0\n  0\nENDSEC\n  0\nSECTION\n"
                    "  2\nOBJECTS\nnot a group code\n  0\nENDSEC\n  0\nEOF\n\n";
        }
        geode::OpenGeodeIOMeshException::test(
            geode::is_polygonal_surface_loadable< 3 >( filename ).value() == 1,
            "A DXF with trailing content should be loadable" );
        auto surface = geode::load_one(
            geode::load_polygonal_surface< 3 >( filename ), filename );
        check_surface( *surface, "LAYER", 3, 1 );

        const std::string eof_filename{ "trailing_blank_line.dxf" };
        {
            std::ofstream file{ eof_filename, std::ios::binary };
            file << "  0\nSECTION\n  2\nENTITIES\n  0\n3DFACE\n  8\nLAYER\n"
                    " 10\n0.0\n 20\n0.0\n 30\n0.0\n 11\n1.0\n 21\n0.0\n"
                    " 31\n0.0\n 12\n0.0\n 22\n1.0\n 32\n0.0\n  0\nEOF\n\n";
        }
        geode::OpenGeodeIOMeshException::test(
            geode::is_polygonal_surface_loadable< 3 >( eof_filename ).value()
                == 1,
            "A DXF with a blank line after EOF should be loadable" );
        auto eof_surface = geode::load_one(
            geode::load_polygonal_surface< 3 >( eof_filename ), eof_filename );
        check_surface( *eof_surface, "LAYER", 3, 1 );

        const std::string endsec_filename{ "trailing_after_endsec.dxf" };
        {
            std::ofstream file{ endsec_filename, std::ios::binary };
            file << "  0\nSECTION\n  2\nENTITIES\n  0\n3DFACE\n  8\nLAYER\n"
                    " 10\n0.0\n 20\n0.0\n 30\n0.0\n 11\n1.0\n 21\n0.0\n"
                    " 31\n0.0\n 12\n0.0\n 22\n1.0\n 32\n0.0\n  0\nENDSEC\n"
                    "\n";
        }
        auto endsec_surface = geode::load_one(
            geode::load_polygonal_surface< 3 >( endsec_filename ),
            endsec_filename );
        check_surface( *endsec_surface, "LAYER", 3, 1 );
    }

    void test_no_face()
    {
        const std::string filename{ "no_face.dxf" };
        {
            std::ofstream file{ filename, std::ios::binary };
            file << "  0\nSECTION\n  2\nENTITIES\n  0\nLINE\n  8\n0\n 10\n0.0\n"
                    " 20\n0.0\n 30\n0.0\n 11\n1.0\n 21\n1.0\n 31\n1.0\n"
                    "  0\nENDSEC\n  0\nEOF\n";
        }
        geode::OpenGeodeIOMeshException::test(
            geode::is_polygonal_surface_loadable< 3 >( filename ).value() == 0,
            "A DXF without faces should not be loadable" );
        expect_data_error( filename, "No object found" );
    }
    void test_3dface_corners()
    {
        // No 13/23/33 codes: the fourth corner is the third one
        const std::string filename{ "3dface_corners.dxf" };
        write_file( filename,
            "  0\nSECTION\n  2\nENTITIES\n  0\n3DFACE\n  8\nTRIANGLE\n"
            " 10\n5.0\n 20\n5.0\n 30\n5.0\n 11\n6.0\n 21\n5.0\n 31\n5.0\n"
            " 12\n5.0\n 22\n6.0\n 32\n5.0\n"
            // Second corner without z: incomplete face
            "  0\n3DFACE\n  8\nINCOMPLETE\n"
            " 10\n5.0\n 20\n5.0\n 30\n5.0\n 11\n6.0\n 21\n5.0\n"
            " 12\n5.0\n 22\n6.0\n 32\n5.0\n 13\n5.0\n 23\n6.0\n 33\n5.0\n"
            "  0\nENDSEC\n  0\nEOF\n" );
        WarningRecorder::start();
        auto surface = geode::load_one(
            geode::load_polygonal_surface< 3 >( filename ), filename );
        check_surface( *surface, "TRIANGLE", 3, 1 );
        for( const auto v : geode::Range{ surface->nb_vertices() } )
        {
            geode::OpenGeodeIOMeshException::test(
                surface->point( v ) != geode::Point3D{ { 0, 0, 0 } },
                "A 3DFACE without fourth corner should not have a vertex at "
                "the origin" );
        }
        geode::OpenGeodeIOMeshException::test(
            WarningRecorder::has_warning( "1 faces skipped" )
                && WarningRecorder::has_warning(
                    "1 with less than 3 distinct vertices" ),
            "The incomplete 3DFACE should be reported as skipped" );
    }

    void test_polyface_minimum_index()
    {
        const std::string filename{ "polyface_minimum_index.dxf" };
        write_file( filename,
            "  0\nSECTION\n  2\nENTITIES\n  0\nPOLYLINE\n  8\nLAYER\n"
            " 70\n64\n"
            "  0\nVERTEX\n 10\n0.0\n 20\n0.0\n 30\n0.0\n 70\n192\n"
            "  0\nVERTEX\n 10\n1.0\n 20\n0.0\n 30\n0.0\n 70\n192\n"
            "  0\nVERTEX\n 10\n0.0\n 20\n1.0\n 30\n0.0\n 70\n192\n"
            "  0\nVERTEX\n 70\n128\n 71\n1\n 72\n2\n 73\n3\n"
            "  0\nVERTEX\n 70\n128\n 71\n1\n 72\n2\n 73\n-2147483648\n"
            "  0\nSEQEND\n  0\nENDSEC\n  0\nEOF\n" );
        WarningRecorder::start();
        auto surface = geode::load_one(
            geode::load_polygonal_surface< 3 >( filename ), filename );
        check_surface( *surface, "LAYER", 3, 1 );
        geode::OpenGeodeIOMeshException::test(
            WarningRecorder::has_warning(
                "1 with out of range vertex indices" ),
            "The face with index -2147483648 should be out of range" );
    }

    void test_missing_seqend()
    {
        // The POLYLINE has no SEQEND: the next entity is still read
        const std::string filename{ "missing_seqend.dxf" };
        write_file( filename,
            "  0\nSECTION\n  2\nENTITIES\n  0\nPOLYLINE\n  8\nPOLYFACE\n"
            "  5\n2A\n 70\n64\n"
            "  0\nVERTEX\n 10\n0.0\n 20\n0.0\n 30\n0.0\n 70\n192\n"
            "  0\nVERTEX\n 10\n1.0\n 20\n0.0\n 30\n0.0\n 70\n192\n"
            "  0\nVERTEX\n 10\n0.0\n 20\n1.0\n 30\n0.0\n 70\n192\n"
            "  0\nVERTEX\n 70\n128\n 71\n1\n 72\n2\n 73\n3\n"
            "  0\n3DFACE\n  8\nFACES\n"
            " 10\n5.0\n 20\n5.0\n 30\n5.0\n 11\n6.0\n 21\n5.0\n 31\n5.0\n"
            " 12\n5.0\n 22\n6.0\n 32\n5.0\n 13\n5.0\n 23\n6.0\n 33\n5.0\n"
            "  0\nENDSEC\n  0\nEOF\n" );
        const auto surfaces = geode::load_polygonal_surface< 3 >( filename );
        geode::OpenGeodeIOMeshException::test( surfaces.size() == 2,
            "Wrong number of surfaces without SEQEND: ", surfaces.size() );
        check_surface( *surfaces[0], "POLYFACE_2A", 3, 1 );
        check_surface( *surfaces[1], "FACES", 3, 1 );
    }

    void test_utf8_bom()
    {
        const std::string filename{ "utf8_bom.dxf" };
        write_file( filename,
            "\xEF\xBB\xBF  0\nSECTION\n  2\nENTITIES\n  0\n3DFACE\n  8\nBOM\n"
            " 10\n5.0\n 20\n5.0\n 30\n5.0\n 11\n6.0\n 21\n5.0\n 31\n5.0\n"
            " 12\n5.0\n 22\n6.0\n 32\n5.0\n 13\n5.0\n 23\n6.0\n 33\n5.0\n"
            "  0\nENDSEC\n  0\nEOF\n" );
        auto surface = geode::load_one(
            geode::load_polygonal_surface< 3 >( filename ), filename );
        check_surface( *surface, "BOM", 3, 1 );
    }

    void test_comment_before_entities()
    {
        const std::string filename{ "comment_before_entities.dxf" };
        write_file( filename,
            "  0\nSECTION\n  2\nENTITIES\n999\nA comment\n  0\n3DFACE\n"
            "  8\nCOMMENTED\n"
            " 10\n5.0\n 20\n5.0\n 30\n5.0\n 11\n6.0\n 21\n5.0\n 31\n5.0\n"
            " 12\n5.0\n 22\n6.0\n 32\n5.0\n 13\n5.0\n 23\n6.0\n 33\n5.0\n"
            "  0\nENDSEC\n  0\nEOF\n" );
        auto surface = geode::load_one(
            geode::load_polygonal_surface< 3 >( filename ), filename );
        check_surface( *surface, "COMMENTED", 3, 1 );
    }

    void test_unicode_layer_names()
    {
        const std::string filename{ "unicode_layers.dxf" };
        write_file( filename,
            "  0\nSECTION\n  2\nENTITIES\n  0\n3DFACE\n  8\nCouche\\U+00E9\n"
            " 10\n5.0\n 20\n5.0\n 30\n5.0\n 11\n6.0\n 21\n5.0\n 31\n5.0\n"
            " 12\n5.0\n 22\n6.0\n 32\n5.0\n 13\n5.0\n 23\n6.0\n 33\n5.0\n"
            "  0\nPOLYLINE\n  8\nPoly\\U+00E9\n  5\n2A\n 70\n64\n"
            "  0\nVERTEX\n 10\n0.0\n 20\n0.0\n 30\n0.0\n 70\n192\n"
            "  0\nVERTEX\n 10\n1.0\n 20\n0.0\n 30\n0.0\n 70\n192\n"
            "  0\nVERTEX\n 10\n0.0\n 20\n1.0\n 30\n0.0\n 70\n192\n"
            "  0\nVERTEX\n 70\n128\n 71\n1\n 72\n2\n 73\n3\n"
            "  0\nSEQEND\n  0\nENDSEC\n  0\nEOF\n" );
        const auto surfaces = geode::load_polygonal_surface< 3 >( filename );
        geode::OpenGeodeIOMeshException::test( surfaces.size() == 2,
            "Wrong number of surfaces with unicode layers: ", surfaces.size() );
        check_surface( *surfaces[0], "Couche\xC3\xA9", 3, 1 );
        check_surface( *surfaces[1], "Poly\xC3\xA9_2A", 3, 1 );
    }

    void test_viewport_not_reported()
    {
        const std::string filename{ "viewport.dxf" };
        write_file( filename,
            "  0\nSECTION\n  2\nENTITIES\n  0\nVIEWPORT\n  8\n0\n 10\n0.0\n"
            " 20\n0.0\n 30\n0.0\n  0\nLINE\n  8\n0\n 10\n0.0\n 20\n0.0\n"
            " 30\n0.0\n 11\n1.0\n 21\n1.0\n 31\n1.0\n  0\n3DFACE\n  8\nVIEW\n"
            " 10\n5.0\n 20\n5.0\n 30\n5.0\n 11\n6.0\n 21\n5.0\n 31\n5.0\n"
            " 12\n5.0\n 22\n6.0\n 32\n5.0\n 13\n5.0\n 23\n6.0\n 33\n5.0\n"
            "  0\nENDSEC\n  0\nEOF\n" );
        WarningRecorder::start();
        auto surface = geode::load_one(
            geode::load_polygonal_surface< 3 >( filename ), filename );
        check_surface( *surface, "VIEW", 3, 1 );
        geode::OpenGeodeIOMeshException::test(
            WarningRecorder::has_warning( "LINE (1)" ),
            "Skipped LINE should be reported" );
        geode::OpenGeodeIOMeshException::test(
            !WarningRecorder::has_warning( "VIEWPORT" ),
            "Skipped VIEWPORT should not be reported" );
    }

    void check_same_triangles( const geode::TriangulatedSurface3D& expected,
        const geode::PolygonalSurface3D& reloaded )
    {
        geode::OpenGeodeIOMeshException::test(
            reloaded.nb_polygons() == expected.nb_polygons(),
            "Wrong number of reloaded triangles: ", reloaded.nb_polygons() );
        for( const auto p : geode::Range{ expected.nb_polygons() } )
        {
            geode::OpenGeodeIOMeshException::test(
                reloaded.nb_polygon_vertices( p ) == 3, "Reloaded polygon ", p,
                " is not a triangle" );
            for( const auto v : geode::LRange{ 3 } )
            {
                geode::OpenGeodeIOMeshException::test(
                    reloaded.point( reloaded.polygon_vertex( { p, v } ) )
                        .inexact_equal( expected.point(
                            expected.polygon_vertex( { p, v } ) ) ),
                    "Wrong position of vertex ", v, " of reloaded triangle ",
                    p );
            }
        }
    }

    void test_triangulated_output()
    {
        const auto filename =
            absl::StrCat( geode::DATA_PATH, "galeries_and_cylinder.og_tsf3d" );
        const auto surface = geode::load_one(
            geode::load_triangulated_surface< 3 >( filename ), filename );
        const std::string output{ "galeries_and_cylinder.dxf" };
        geode::save_triangulated_surface( *surface, output );

        const auto reloaded = geode::load_one(
            geode::load_polygonal_surface< 3 >( output ), output );
        geode::OpenGeodeIOMeshException::test(
            reloaded->nb_vertices() == surface->nb_vertices(),
            "Wrong number of reloaded vertices: ", reloaded->nb_vertices(),
            " instead of ", surface->nb_vertices() );
        check_same_triangles( *surface, *reloaded );
    }

    void test_triangulated_output_precision_and_name()
    {
        auto surface = geode::TriangulatedSurface3D::create();
        auto builder = geode::TriangulatedSurfaceBuilder3D::create( *surface );
        // Forbidden characters in layer names are replaced, others kept
        builder->set_name( "Galerie \xC3\xA9:1" );
        builder->create_point(
            geode::Point3D{ { 650000.123456789, 4100000.987654321, -0.1 } } );
        builder->create_point(
            geode::Point3D{ { 650010.321, 4100000.654, 100.987 } } );
        builder->create_point(
            geode::Point3D{ { 650000.111, 4100010.222, 101.333 } } );
        builder->create_point(
            geode::Point3D{ { 650010.5, 4100010.5, 1e-7 } } );
        builder->create_triangle( { 0, 1, 2 } );
        builder->create_triangle( { 2, 1, 3 } );
        const std::string output{ "triangulated_output.dxf" };
        geode::save_triangulated_surface( *surface, output );

        const auto reloaded = geode::load_one(
            geode::load_polygonal_surface< 3 >( output ), output );
        check_surface( *reloaded, "Galerie \xC3\xA9_1", 4, 2 );
        check_same_triangles( *surface, *reloaded );
        for( const auto v : geode::Range{ surface->nb_vertices() } )
        {
            bool found{ false };
            for( const auto r : geode::Range{ reloaded->nb_vertices() } )
            {
                found = found || reloaded->point( r ) == surface->point( v );
            }
            geode::OpenGeodeIOMeshException::test( found, "Point ",
                surface->point( v ).string(),
                " lost precision in DXF round trip" );
        }
        check_adjacency( *reloaded, { 0, 1 }, 1 );
    }

    void test_polygonal_output()
    {
        auto surface = geode::PolygonalSurface3D::create();
        auto builder = geode::PolygonalSurfaceBuilder3D::create( *surface );
        // 3-byte UTF-8 character: encoded by the writer, decoded by the reader
        builder->set_name( "Baie \xE2\x82\xAC" );
        const std::array< geode::Point3D, 5 > points{ {
            geode::Point3D{ { 0, 0, 0 } },
            geode::Point3D{ { 1, 0, 0.5 } },
            geode::Point3D{ { 1, 1, 0.5 } },
            geode::Point3D{ { 0, 1, 0 } },
            geode::Point3D{ { 2, 0.5, 1 } },
        } };
        for( const auto& point : points )
        {
            builder->create_point( point );
        }
        builder->create_polygon( { 0, 1, 2, 3 } );
        builder->create_polygon( { 1, 4, 2 } );
        const std::string output{ "polygonal_output.dxf" };
        geode::save_polygonal_surface( *surface, output );

        const auto reloaded = geode::load_one(
            geode::load_polygonal_surface< 3 >( output ), output );
        check_surface( *reloaded, "Baie \xE2\x82\xAC", 5, 2 );
        for( const auto p : geode::Range{ surface->nb_polygons() } )
        {
            const auto nb_vertices = surface->nb_polygon_vertices( p );
            geode::OpenGeodeIOMeshException::test(
                reloaded->nb_polygon_vertices( p ) == nb_vertices,
                "Wrong size of reloaded polygon ", p );
            for( const auto v : geode::LRange{ nb_vertices } )
            {
                geode::OpenGeodeIOMeshException::test(
                    reloaded->point( reloaded->polygon_vertex( { p, v } ) )
                        == surface->point(
                            surface->polygon_vertex( { p, v } ) ),
                    "Wrong position of vertex ", v, " of reloaded polygon ",
                    p );
            }
        }
        check_adjacency( *reloaded, { 0, 1 }, 1 );
        check_adjacency( *reloaded, { 1, 2 }, 0 );
    }

    void test_polygonal_output_too_many_vertices()
    {
        auto surface = geode::PolygonalSurface3D::create();
        auto builder = geode::PolygonalSurfaceBuilder3D::create( *surface );
        builder->create_point( geode::Point3D{ { 0, 0, 0 } } );
        builder->create_point( geode::Point3D{ { 1, 0, 0 } } );
        builder->create_point( geode::Point3D{ { 2, 1, 0 } } );
        builder->create_point( geode::Point3D{ { 1, 2, 0 } } );
        builder->create_point( geode::Point3D{ { 0, 1, 0 } } );
        builder->create_polygon( { 0, 1, 2, 3, 4 } );
        const std::string output{ "pentagon.dxf" };
        try
        {
            geode::save_polygonal_surface( *surface, output );
        }
        catch( const geode::OpenGeodeException& exception )
        {
            geode::OpenGeodeIOMeshException::test(
                exception.type() == geode::OpenGeodeException::TYPE::data
                    && absl::StrContains(
                        exception.what(), "a 3DFACE has at most 4" ),
                "Wrong error saving a pentagon: ", exception.what() );
            geode::OpenGeodeIOMeshException::test(
                !std::ifstream{ output }.good(),
                "No file should be written for a pentagon" );
            return;
        }
        throw geode::OpenGeodeIOMeshException{ nullptr,
            geode::OpenGeodeException::TYPE::internal,
            "Saving a pentagon as DXF should raise a data error" };
    }

    /* Saves a triangle named name and returns the name of the reloaded one */
    std::string reloaded_layer_name( std::string_view name )
    {
        auto surface = geode::TriangulatedSurface3D::create();
        auto builder = geode::TriangulatedSurfaceBuilder3D::create( *surface );
        if( !name.empty() )
        {
            builder->set_name( name );
        }
        builder->create_point( geode::Point3D{ { 0, 0, 0 } } );
        builder->create_point( geode::Point3D{ { 1, 0, 0 } } );
        builder->create_point( geode::Point3D{ { 0, 1, 0 } } );
        builder->create_triangle( { 0, 1, 2 } );
        const std::string output{ "layer_name.dxf" };
        geode::save_triangulated_surface( *surface, output );
        const auto reloaded = geode::load_one(
            geode::load_polygonal_surface< 3 >( output ), output );
        return std::string{ reloaded->name().value_or( "" ) };
    }

    void check_layer_name(
        std::string_view name, std::string_view expected, std::string_view why )
    {
        const auto reloaded = reloaded_layer_name( name );
        geode::OpenGeodeIOMeshException::test( reloaded == expected, why,
            ": \"", reloaded, "\" instead of \"", expected, "\"" );
    }

    void test_output_layer_names()
    {
        check_layer_name(
            "A,B", "A_B", "Comma is forbidden in DXF layer names" );
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        test_polyface_surfaces();
        test_3d_faces();
        test_face_and_line();
        test_binary_dxf();
        test_trailing_content();
        test_no_face();
        test_3dface_corners();
        test_polyface_minimum_index();
        test_missing_seqend();
        test_utf8_bom();
        test_comment_before_entities();
        test_unicode_layer_names();
        test_viewport_not_reported();
        test_triangulated_output();
        test_triangulated_output_precision_and_name();
        test_polygonal_output();
        test_polygonal_output_too_many_vertices();
        test_output_layer_names();

        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
