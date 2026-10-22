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

#include <filesystem>
#include <string>
#include <vector>

#include <absl/strings/str_cat.h>

#include <geode/basic/assert.hpp>
#include <geode/basic/logger.hpp>

#include <geode/model/mixin/core/block.hpp>
#include <geode/model/mixin/core/corner.hpp>
#include <geode/model/mixin/core/line.hpp>
#include <geode/model/mixin/core/surface.hpp>
#include <geode/model/representation/core/brep.hpp>
#include <geode/model/representation/core/section.hpp>
#include <geode/model/representation/io/brep_input.hpp>
#include <geode/model/representation/io/brep_output.hpp>
#include <geode/model/representation/io/section_input.hpp>
#include <geode/model/representation/io/section_output.hpp>

#include <geode/io/mesh/common.hpp>
#include <geode/io/model/common.hpp>

namespace
{
    void check_files(
        const std::vector< std::string >& files, geode::index_t nb_components )
    {
        geode::OpenGeodeIOModelException::test(
            files.size() == nb_components + 1,
            "VTM output should list the .vtm file and one file per "
            "component, got ",
            files.size(), " files for ", nb_components, " components" );
        for( const auto& file : files )
        {
            geode::OpenGeodeIOModelException::test(
                std::filesystem::exists( file ), "Listed file ", file,
                " should exist" );
        }
    }
} // namespace

int main()
{
    try
    {
        geode::OpenGeodeIOMeshLibrary::initialize();
        geode::OpenGeodeIOModelLibrary::initialize();

        auto brep =
            geode::load_brep( absl::StrCat( geode::DATA_PATH, "mss.og_brep" ) );
        const auto filename = absl::StrCat( "mss.vtm" );
        check_files( geode::save_brep( brep, filename ),
            brep.nb_corners() + brep.nb_lines() + brep.nb_surfaces()
                + brep.nb_blocks() );
        auto section = geode::load_section(
            absl::StrCat( geode::DATA_PATH, "mss_cut_section.og_sctn" ) );
        const auto filename2 = absl::StrCat( "mss_cut_section.vtm" );
        check_files( geode::save_section( section, filename2 ),
            section.nb_corners() + section.nb_lines() + section.nb_surfaces() );
        geode::Logger::info( "TEST SUCCESS" );
        return 0;
    }
    catch( ... )
    {
        return geode::geode_lippincott();
    }
}
