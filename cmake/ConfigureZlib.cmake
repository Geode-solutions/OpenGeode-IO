# Copyright (c) 2019 - 2020 Geode-solutions
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set(ZLIB_PATH ${PROJECT_BINARY_DIR}/third_party/zlib)
set(ZLIB_INSTALL_PREFIX ${ZLIB_PATH}/install)
string(REPLACE "/MDd" "/MD" NEW_FLAGS ${CMAKE_CXX_FLAGS_DEBUG})
ExternalProject_Add(zlib
    PREFIX ${ZLIB_PATH}
    GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng
    GIT_PROGRESS ON
    GIT_TAG fe69810c265858b7b4242663d51336726f4a98be
    CMAKE_GENERATOR ${CMAKE_GENERATOR}
    CMAKE_GENERATOR_PLATFORM ${CMAKE_GENERATOR_PLATFORM}
    CMAKE_ARGS
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_INSTALL_MESSAGE=LAZY
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    CMAKE_CACHE_ARGS
        -DCMAKE_C_FLAGS_DEBUG:INTERNAL=${NEW_FLAGS}
        -DCMAKE_CXX_FLAGS_DEBUG:INTERNAL=${NEW_FLAGS}
        -DZLIB_ENABLE_TESTS:BOOL=OFF
        -DCMAKE_INSTALL_PREFIX:PATH=${ZLIB_INSTALL_PREFIX}
)
