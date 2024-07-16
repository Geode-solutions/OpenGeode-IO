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

#include <geode/io/mesh/internal/smesh_curve_input.hpp>

#include <fstream>

#include <absl/strings/str_split.h>

#include <geode/basic/filename.hpp>

#include <geode/geometry/point.hpp>

#include <geode/mesh/builder/edged_curve_builder.hpp>
#include <geode/mesh/core/edged_curve.hpp>

#include <geode/io/mesh/internal/smesh_input.hpp>

namespace
{
    class SMESHCurveInputImpl
        : public geode::internal::SMESHInputImpl< geode::EdgedCurve3D,
              geode::EdgedCurveBuilder3D,
              2 >
    {
    public:
        SMESHCurveInputImpl(
            std::string_view filename, geode::EdgedCurve3D& curve )
            : geode::internal::SMESHInputImpl< geode::EdgedCurve3D,
                  geode::EdgedCurveBuilder3D,
                  2 >( filename, curve )
        {
        }

    private:
        void create_element(
            const std::array< geode::index_t, 2 >& vertices ) override
        {
            builder().create_edge( vertices[0], vertices[1] );
        }
    };
} // namespace

namespace geode
{
    namespace internal
    {
        std::unique_ptr< EdgedCurve3D > SMESHCurveInput::read(
            const MeshImpl& impl )
        {
            auto curve = EdgedCurve3D::create( impl );
            SMESHCurveInputImpl reader{ filename(), *curve };
            reader.read_file();
            return curve;
        }
    } // namespace internal
} // namespace geode
