/*
 * Copyright (c) 2019 - 2023 Geode-solutions
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

#include <geode/io/model/private/vtm_section_output.h>

#include <geode/model/representation/core/section.h>

#include <geode/io/model/private/vtm_output.h>

namespace
{
    class VTMSectionOutputImpl
        : public geode::detail::VTMOutputImpl< geode::Section, 2 >
    {
    public:
        VTMSectionOutputImpl(
            absl::string_view filename, const geode::Section& section )
            : geode::detail::VTMOutputImpl< geode::Section, 2 >{ filename,
                  section }
        {
        }

    private:
        void write_piece( pugi::xml_node& object ) final
        {
            this->write_corners_lines_surfaces( object );
        }
    };
} // namespace

namespace geode
{
    namespace detail
    {
        void VTMSectionOutput::write( const Section& section ) const
        {
            VTMSectionOutputImpl impl{ filename(), section };
            impl.write_file();
        }
    } // namespace detail
} // namespace geode
