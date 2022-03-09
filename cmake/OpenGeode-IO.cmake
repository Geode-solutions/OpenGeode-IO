# Copyright (c) 2019 - 2022 Geode-solutions
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

# Define the project
project(OpenGeode-IO CXX)

# Get OpenGeode-IO dependencies
find_package(OpenGeode REQUIRED)
find_package(assimp REQUIRED CONFIG NO_DEFAULT_PATH PATHS ${ASSIMP_INSTALL_PREFIX})
find_package(pugixml REQUIRED CONFIG NO_DEFAULT_PATH PATHS ${PUGIXML_INSTALL_PREFIX})
find_package(zlib REQUIRED CONFIG NO_DEFAULT_PATH PATHS ${ZLIB_INSTALL_PREFIX})

#------------------------------------------------------------------------------------------------
# Configure the OpenGeode-IO libraries
add_subdirectory(src/geode)

#------------------------------------------------------------------------------------------------
# Optional modules configuration
if(OPENGEODE_IO_WITH_TESTS)
    # Enable testing with CTest
    enable_testing()
    message(STATUS "Configuring OpenGeode-IO with tests")
    add_subdirectory(tests)
endif()

if(OPENGEODE_IO_WITH_PYTHON)
    message(STATUS "Configuring OpenGeode-IO with Python bindings")
    add_subdirectory(bindings/python)
endif()

#------------------------------------------------------------------------------------------------
# Configure CPack
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

# This must always be last!
include(CPack)
