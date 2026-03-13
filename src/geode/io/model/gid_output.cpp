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


#include<geode/io/model/internal/gid_output.hpp>

#include <fstream>
#include <string>
#include <vector>

#include <geode/mesh/core/tetrahedral_solid.hpp>
#include <geode/mesh/core/triangulated_surface.hpp>

#include <geode/model/representation/core/brep.hpp>
#include <geode/model/mixin/core/block.hpp>
#include <geode/model/mixin/core/surface.hpp>

namespace
{
    static constexpr char EOL{ '\n' };
    static constexpr char SPACE{ ' ' };
    static constexpr geode::index_t DEFAULT_PHYSICAL_TAG{ 0 };

    class GIDOutputImpl
    {
    public:
        GIDOutputImpl( std::string_view filename, const geode::BRep& brep )
            : file_{ geode::to_string( filename ) }, brep_( brep )
        {
            OPENGEODE_EXCEPTION( file_.good(),
                "[GIDOutput] Error while opening file: ", filename );
        }

        void write_file()
        {
            write_header();
        }

    private:

        void write_header()
        {
            file_ << "MESH";
            file_ << SPACE << "dimension"<< SPACE << 3;
            file_ << SPACE << "Tetrahedra" << SPACE << "Nnode" << SPACE << 4 << EOL;
        }

     private:
        std::ofstream file_;
        const geode::BRep& brep_;
    };
} // namespace

namespace geode
{
    namespace internal
    {
        std::vector< std::string > GIDOutput::write( const BRep& brep ) const
        {
            GIDOutputImpl impl( filename(), brep );
            impl.write_file();
            return { to_string( filename() ) };
        }

        bool GIDOutput::is_saveable( const BRep& brep ) const
        {
            for( const auto& surface : brep.surfaces() )
            {
                if( surface.mesh().type_name()
                    != TriangulatedSurface3D::type_name_static() )
                {
                    return false;
                }
            }
            for( const auto& block : brep.blocks() )
            {
                if( block.mesh().type_name()
                    != TetrahedralSolid3D::type_name_static() )
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace internal
} // namespace geode
