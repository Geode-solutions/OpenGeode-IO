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

#include <geode/io/mesh/internal/pnt_input.hpp>

#include <cmath>
#include <fstream>
#include <optional>
#include <string>

#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

#include <geode/basic/attribute_manager.hpp>
#include <geode/basic/string.hpp>
#include <geode/basic/variable_attribute.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/point_set_builder.hpp>
#include <geode/mesh/core/point_set.hpp>

#include <geode/io/mesh/common.hpp>

namespace
{
    constexpr char COMMENT_MARKER{ '*' };
    constexpr geode::index_t NEW_POLYLINE_FLAG{ 3 };
    constexpr geode::index_t MAX_FLAG{ 3 };
    constexpr geode::index_t NB_MANDATORY_VALUES{ 4 };
    constexpr geode::index_t COLOUR_COLUMN{ 4 };
    constexpr geode::index_t MTYPE_COLUMN{ 5 };

    using IndexAttribute = geode::VariableAttribute< geode::index_t >;

    struct PNTRecord
    {
        geode::Point3D point;
        geode::index_t flag{ 0 };
        std::optional< geode::index_t > colour;
        std::optional< geode::index_t > mtype;
    };

    class PNTInputImpl
    {
    public:
        explicit PNTInputImpl( std::string_view filename )
            : filename_{ filename },
              file_{ geode::to_string( filename ), std::ios::binary }
        {
        }

        std::unique_ptr< geode::PointSet3D > read_point_set(
            const geode::MeshImpl& impl )
        {
            geode::OpenGeodeIOMeshException::check_exception( file_.good(),
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[PNTInput] Cannot open file ", filename_ );
            auto point_set = geode::PointSet3D::create( impl );
            auto builder = geode::PointSetBuilder3D::create( *point_set );
            auto& manager = point_set->vertex_attribute_manager();
            auto polyline_attribute =
                create_index_attribute( manager, "polyline" );
            auto flag_attribute = create_index_attribute( manager, "flag" );
            std::shared_ptr< IndexAttribute > colour_attribute;
            std::shared_ptr< IndexAttribute > mtype_attribute;
            geode::index_t nb_polylines{ 0 };
            std::optional< geode::index_t > polyline_colour;
            std::optional< geode::index_t > polyline_mtype;
            while( const auto record = next_record() )
            {
                if( record->flag == NEW_POLYLINE_FLAG || nb_polylines == 0 )
                {
                    nb_polylines++;
                    polyline_colour = record->colour;
                    polyline_mtype = record->mtype;
                }
                const auto vertex = builder->create_point( record->point );
                polyline_attribute->set_value( vertex, nb_polylines - 1 );
                flag_attribute->set_value( vertex, record->flag );
                set_optional_value( manager, colour_attribute, "colour", vertex,
                    record->colour ? record->colour : polyline_colour );
                set_optional_value( manager, mtype_attribute, "mtype", vertex,
                    record->mtype ? record->mtype : polyline_mtype );
            }
            return point_set;
        }

        geode::Percentage is_loadable()
        {
            static constexpr geode::index_t NB_SAMPLED_RECORDS{ 100 };
            if( !file_.good() )
            {
                return geode::Percentage{ 0 };
            }
            try
            {
                geode::index_t nb_records{ 0 };
                while( nb_records < NB_SAMPLED_RECORDS && next_record() )
                {
                    nb_records++;
                }
                return geode::Percentage{ nb_records == 0 ? 0. : 1. };
            }
            catch( const geode::OpenGeodeException& )
            {
                return geode::Percentage{ 0 };
            }
        }

    private:
        std::optional< PNTRecord > next_record()
        {
            std::string line;
            while( std::getline( file_, line ) )
            {
                line_number_++;
                const auto content = absl::StripAsciiWhitespace( line );
                if( content.empty() || content.front() == COMMENT_MARKER )
                {
                    continue;
                }
                return parse_record( content );
            }
            return std::nullopt;
        }

        [[nodiscard]] PNTRecord parse_record( std::string_view content ) const
        {
            const std::vector< std::string_view > values = absl::StrSplit(
                content, absl::ByAnyChar( " \t" ), absl::SkipEmpty() );
            check_line( values.size() >= NB_MANDATORY_VALUES,
                "expected \"x y "
                "z flag\", "
                "found ",
                values.size(), " values" );
            PNTRecord record;
            for( const auto axis : geode::LRange{ 3 } )
            {
                double coordinate{ 0 };
                check_line( absl::SimpleAtod( values[axis], &coordinate )
                                && std::isfinite( coordinate ),
                    "invalid coordinate \"", values[axis], "\"" );
                record.point.set_value( axis, coordinate );
            }
            record.flag = parse_index( values[3], "flag" );
            check_line( record.flag >= 1 && record.flag <= MAX_FLAG,
                "flag should be 1, 2 or 3, found ", record.flag );
            if( values.size() > COLOUR_COLUMN )
            {
                record.colour = parse_index( values[COLOUR_COLUMN], "colour" );
            }
            if( values.size() > MTYPE_COLUMN )
            {
                record.mtype = parse_index( values[MTYPE_COLUMN], "mtype" );
            }
            return record;
        }

        [[nodiscard]] geode::index_t parse_index(
            std::string_view value, std::string_view name ) const
        {
            geode::index_t index{ 0 };
            check_line( absl::SimpleAtoi( value, &index ), "invalid ", name,
                " \"", value, "\"" );
            return index;
        }

        template < typename... Args >
        void check_line( bool condition, const Args&... message ) const
        {
            geode::OpenGeodeIOMeshException::check_exception( condition,
                nullptr, geode::OpenGeodeException::TYPE::data,
                "[PNTInput] Line ", line_number_, " of file ", filename_, ": ",
                message... );
        }

        static std::shared_ptr< IndexAttribute > create_index_attribute(
            geode::AttributeManager& manager, std::string_view name )
        {
            geode::AttributeValues< geode::index_t > values;
            values.default_value = geode::NO_ID;
            values.no_value = geode::NO_ID;
            geode::AttributeProperties properties;
            properties.assignable = false;
            properties.interpolable = false;
            properties.transferable = true;
            const auto attribute_id =
                manager.create_attribute< geode::VariableAttribute,
                    geode::index_t >( name, values, properties );
            return manager
                .find_attribute< geode::VariableAttribute, geode::index_t >(
                    attribute_id );
        }

        static void set_optional_value( geode::AttributeManager& manager,
            std::shared_ptr< IndexAttribute >& attribute,
            std::string_view name,
            geode::index_t vertex,
            std::optional< geode::index_t > value )
        {
            if( !value )
            {
                return;
            }
            if( !attribute )
            {
                attribute = create_index_attribute( manager, name );
            }
            attribute->set_value( vertex, value.value() );
        }

    private:
        std::string_view filename_;
        std::ifstream file_;
        geode::index_t line_number_{ 0 };
    };
} // namespace

namespace geode::internal
{
    std::vector< std::unique_ptr< PointSet3D > > PNTInput::read(
        const MeshImpl& impl )
    {
        PNTInputImpl reader{ filename() };
        std::vector< std::unique_ptr< PointSet3D > > point_sets;
        point_sets.emplace_back( reader.read_point_set( impl ) );
        return point_sets;
    }

    Percentage PNTInput::is_loadable() const
    {
        PNTInputImpl reader{ filename() };
        return reader.is_loadable();
    }
} // namespace geode::internal
