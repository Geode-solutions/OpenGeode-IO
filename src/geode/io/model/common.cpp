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

#include <geode/io/model/detail/common.h>

#include <geode/io/model/private/msh_input.h>
#include <geode/io/model/private/msh_output.h>
#include <geode/io/model/private/svg_input.h>

namespace
{
    void register_brep_input()
    {
        geode::BRepInputFactory::register_creator< geode::detail::MSHInput >(
            geode::detail::MSHInput::extension().data() );
    }

    void register_brep_output()
    {
        geode::BRepOutputFactory::register_creator< geode::detail::MSHOutput >(
            geode::detail::MSHOutput::extension().data() );
    }

    void register_section_input()
    {
        geode::SectionInputFactory::register_creator< geode::detail::SVGInput >(
            geode::detail::SVGInput::extension().data() );
    }

    OPENGEODE_LIBRARY_INITIALIZE( OpenGeode_IO_model )
    {
        register_brep_input();
        register_section_input();

        register_brep_output();
    }
} // namespace

namespace geode
{
    namespace detail
    {
        void initialize_model_io()
        {
            Logger::info( "Initializing OpenGeode-IO model library" );
        }
    } // namespace detail
} // namespace geode