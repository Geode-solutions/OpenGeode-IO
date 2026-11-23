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

#include <geode/io/mesh/internal/dxf_surface_output.hpp>

#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <string>

#include <geode/geometry/point.hpp>

#include <geode/mesh/core/surface_mesh.hpp>

#include <geode/io/mesh/common.hpp>
#include <geode/io/mesh/internal/dxf_common.hpp>

namespace
{
    constexpr std::string_view DEFAULT_LAYER{ "0" };
    constexpr std::string_view FORBIDDEN_LAYER_CHARACTERS{ "<>/\\\":;?*|,=`" };
    constexpr unsigned char FIRST_PRINTABLE{ 0x20 };

    /*!
     * Layer names cannot contain some characters, which are replaced by '_'.
     * Non-ASCII characters are written as \U+XXXX escapes.
     * The R12 restrictions (31 characters among A-Z 0-9 $ - _) are not
     * applied, to keep the surface name: see write_dxf_surface.
     */
    std::string layer_name( std::string_view name )
    {
        if( name.empty() )
        {
            return std::string{ DEFAULT_LAYER };
        }
        // Replaced before encoding: the '\' of the escapes is kept
        std::string layer{ name };
        for( auto& character : layer )
        {
            // Only ASCII characters match: UTF-8 sequences are kept
            if( static_cast< unsigned char >( character ) < FIRST_PRINTABLE
                || FORBIDDEN_LAYER_CHARACTERS.find( character )
                       != std::string_view::npos )
            {
                character = '_';
            }
        }
        return geode::internal::encode_unicode_escapes( layer );
    }

    class DXFSurfaceWriter
    {
    public:
        DXFSurfaceWriter(
            std::string_view filename, const geode::SurfaceMesh3D& mesh )
            : filename_{ filename }, mesh_( mesh )
        {
            // Checked before creating the file: no partial file is left
            for( const auto polygon : geode::Range{ mesh_.nb_polygons() } )
            {
                const auto nb_vertices = mesh_.nb_polygon_vertices( polygon );
                geode::OpenGeodeIOMeshException::check_exception(
                    nb_vertices <= geode::internal::NB_3DFACE_CORNERS, nullptr,
                    geode::OpenGeodeException::TYPE::data,
                    "[DXFOutput] Cannot write ", filename_, ": polygon ",
                    polygon, " has ", nb_vertices,
                    " vertices, a 3DFACE has at most ",
                    geode::internal::NB_3DFACE_CORNERS );
            }
            file_.open( geode::to_string( filename ) );
            geode::OpenGeodeIOMeshException::check_exception( file_.good(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[DXFOutput] Error while opening file: ", filename_ );
            file_.imbue( std::locale::classic() );
            file_.precision( std::numeric_limits< double >::max_digits10 );
        }

        void write()
        {
            write_group( 0, "SECTION" );
            write_group( 2, "HEADER" );
            write_group( 9, "$ACADVER" );
            // R12, except for the layer names: see write_dxf_surface
            write_group( 1, "AC1009" );
            write_group( 0, "ENDSEC" );
            write_group( 0, "SECTION" );
            write_group( 2, "ENTITIES" );
            const auto layer = layer_name( mesh_.name().value_or( "" ) );
            for( const auto polygon : geode::Range{ mesh_.nb_polygons() } )
            {
                write_face( polygon, layer );
            }
            write_group( 0, "ENDSEC" );
            write_group( 0, "EOF" );
            file_ << std::flush;
            geode::OpenGeodeIOMeshException::check_exception( file_.good(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[DXFOutput] Error while writing file: ", filename_ );
        }

    private:
        template < typename Value >
        void write_group( int code, const Value& value )
        {
            file_ << std::setw( 3 ) << code << '\n' << value << '\n';
        }

        void write_face( geode::index_t polygon, const std::string& layer )
        {
            write_group( 0, "3DFACE" );
            write_group( 8, layer );
            const auto nb_vertices = mesh_.nb_polygon_vertices( polygon );
            for( const auto corner :
                geode::LRange{ geode::internal::NB_3DFACE_CORNERS } )
            {
                // A triangle repeats its last vertex as fourth corner
                const geode::local_index_t vertex =
                    corner < nb_vertices ? corner : nb_vertices - 1;
                const auto& point =
                    mesh_.point( mesh_.polygon_vertex( { polygon, vertex } ) );
                for( const auto axis : geode::LRange{ 3 } )
                {
                    write_group(
                        10 * ( axis + 1 ) + corner, point.value( axis ) );
                }
            }
        }

    private:
        std::string_view filename_;
        std::ofstream file_;
        const geode::SurfaceMesh3D& mesh_;
    };
} // namespace

namespace geode
{
    namespace internal
    {
        void write_dxf_surface(
            std::string_view filename, const SurfaceMesh3D& surface )
        {
            DXFSurfaceWriter writer{ filename, surface };
            writer.write();
        }
    } // namespace internal
} // namespace geode
