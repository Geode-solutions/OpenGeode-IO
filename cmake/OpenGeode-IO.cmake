# Copyright (c) 2019 Geode-solutions
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
project(OpenGeodeIO CXX)

option(OPENGEODEIO_WITH_TESTS "Compile test projects" ON)

#------------------------------------------------------------------------------------------------
# Platform dependent settings
if(UNIX)
    add_compile_options(-Wall -Wextra -Wno-attributes)
else()
    add_compile_options(/DNOMINMAX)
endif()

# Get OpenGeode-IO dependencies
find_package(OpenGeode REQUIRED)
find_package(assimp REQUIRED CONFIG NO_DEFAULT_PATH PATHS ${ASSIMP_INSTALL_PREFIX})

copy_windows_binaries(assimp)

# Install OpenGeode-IO third-parties
install(
    DIRECTORY
        ${ASSIMP_INSTALL_PREFIX}/
    DESTINATION
        .
)

#------------------------------------------------------------------------------------------------
# Configure the OpenGeode-IO libraries
add_geode_library(geode/mesh)

#------------------------------------------------------------------------------------------------
# Optional modules configuration
if(OPENGEODEIO_WITH_TESTS)
    # Enable testing with CTest
    enable_testing()
    message(STATUS "Configuring OpenGeode-IO with tests")
    add_subdirectory(tests)
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
