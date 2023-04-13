# Copyright (c) 2019 - 2023 Geode-solutions
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

set(OpenGeode-IO_PATH_BIN ${PROJECT_BINARY_DIR}/opengeode-io)
set(OpenGeode-IO_PATH_INSTALL ${OpenGeode-IO_PATH_BIN}/install)
ExternalProject_Add(opengeode-io
    PREFIX ${OpenGeode-IO_PATH_BIN}
    SOURCE_DIR ${PROJECT_SOURCE_DIR}
    CMAKE_GENERATOR ${CMAKE_GENERATOR}
    CMAKE_GENERATOR_PLATFORM ${CMAKE_GENERATOR_PLATFORM}
    CMAKE_ARGS
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}
        -DCPACK_SYSTEM_NAME=${CPACK_SYSTEM_NAME}
        -DCPACK_PACKAGE_VERSION=${CPACK_PACKAGE_VERSION}
        -DCMAKE_INSTALL_MESSAGE=LAZY
        -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
    CMAKE_CACHE_ARGS
        -DWHEEL_VERSION:STRING=${WHEEL_VERSION}
        -DPYTHON_VERSION:STRING=${PYTHON_VERSION}
        -DOPENGEODE_IO_WITH_TESTS:BOOL=${OPENGEODE_IO_WITH_TESTS}
        -DOPENGEODE_IO_WITH_PYTHON:BOOL=${OPENGEODE_IO_WITH_PYTHON}
        -DUSE_SUPERBUILD:BOOL=OFF
        -DASSIMP_INSTALL_PREFIX:PATH=${ASSIMP_INSTALL_PREFIX}
        -DPUGIXML_INSTALL_PREFIX:PATH=${PUGIXML_INSTALL_PREFIX}
        -DZLIB_INSTALL_PREFIX:PATH=${ZLIB_INSTALL_PREFIX}
        -DCMAKE_INSTALL_PREFIX:PATH=${OpenGeode-IO_PATH_INSTALL}
    BINARY_DIR ${OpenGeode-IO_PATH_BIN}
    DEPENDS 
        assimp
        pugixml
        zlib
)

ExternalProject_Add_StepTargets(opengeode-io configure)

add_custom_target(third_party
    DEPENDS
        opengeode-io-configure
)

