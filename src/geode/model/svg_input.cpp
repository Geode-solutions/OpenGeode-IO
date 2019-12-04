/*
 * Copyright (c) 2019 Geode-solutions
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

#include <geode/model/detail/svg_input.h>

#include <fstream>

#include <pugixml.hpp>

#include <geode/geometry/point.h>
#include <geode/model/representation/core/section.h>

namespace
{
    class SVGInputImpl
    {
    public:
        SVGInputImpl( std::string filename, geode::Section& section )
            : file_( std::move( filename ) ),
              section_( section ),
              builder_{ section }
        {
            OPENGEODE_EXCEPTION( file_.good(),
                "[SVGInput] Error while opening file: " + filename );
            auto ok = document_.load_file( filename.c_str() );
            OPENGEODE_EXCEPTION(
                ok, "[SVGInput] Error while parsing file: " + filename );
        }

        void read_file()
        {
            for( const auto& group : document_.child( "svg" ).children( "g" ) )
            {
                for( const auto& path : group.children( "path" ) )
                {
                    paths_.push_back( path.attribute( "d" ).value() );
                }
            }
            DEBUG( paths_.size() );
        }

        void process_paths()
        {
            for( auto& path : paths_ )
            {
                format_path( path );
                auto tokens = split_path( path );
                DEBUG( tokens.size() );
                process_tokens( tokens );
            }
        }

    private:
        void format_path( std::string& path )
        {
            remove_commas( path );
            capitalize_letters( path );
            add_space_around_letters( path );
        }

        std::vector< std::string > split_path( const std::string& path ) const
        {
            std::istringstream iss( path );
            return { std::istream_iterator< std::string >( iss ), {} };
        }

        void process_tokens( const std::vector< std::string >& tokens )
        {
            DEBUG( "--------" );
            if( tokens.size() < 20 )
            {
                for( const auto& t : tokens )
                {
                    DEBUG( t );
                }
            }
        }

        void remove_commas( std::string& path )
        {
            std::replace( path.begin(), path.end(), ',', ' ' );
        }

        void capitalize_letters( std::string& path )
        {
            std::replace( path.begin(), path.end(), 'm', 'M' );
            std::replace( path.begin(), path.end(), 'l', 'L' );
            std::replace( path.begin(), path.end(), 'h', 'H' );
            std::replace( path.begin(), path.end(), 'v', 'V' );
            std::replace( path.begin(), path.end(), 'z', 'Z' );
        }

        void replace_all_occurences( std::string& context,
            const std::string& to_replace,
            const std::string& replacement ) const
        {
            std::size_t look_here = 0;
            std::size_t found_here;
            while( ( found_here = context.find( to_replace, look_here ) )
                   != std::string::npos )
            {
                context.replace( found_here, to_replace.size(), replacement );
                look_here = found_here + replacement.size();
            }
        }

        void add_space_around_letters( std::string& path )
        {
            std::string letters( "MLHVZ" );
            for( const auto letter : letters )
            {
                auto string_letter = std::to_string( letter );
                replace_all_occurences(
                    path, string_letter, " " + string_letter + " " );
            }
        }

    private:
        std::ifstream file_;
        geode::Section& section_;
        geode::SectionBuilder builder_;
        pugi::xml_document document_;
        std::vector< std::string > paths_;
    };
} // namespace

namespace geode
{
    void SVGInput::read()
    {
        SVGInputImpl impl( filename(), section() );
        impl.read_file();
        impl.process_paths();
        // impl.build_topology();
    }
} // namespace geode
