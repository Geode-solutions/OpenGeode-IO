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

cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
cmake_policy(GET CMP0091 AAA)

# Define the project
project(OpenGeode-IO CXX)

if(WIN32)
    if(CMAKE_C_FLAGS_DEBUG)
        string(REPLACE "/MDd" "/MD" CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
    endif()
    if(CMAKE_CXX_FLAGS_DEBUG)
        string(REPLACE "/MDd" "/MD" CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS_DEBUG})
    endif()
endif()

# Get OpenGeode-IO dependencies
find_package(OpenGeode REQUIRED)
find_package(Async++ REQUIRED)
find_package(assimp REQUIRED CONFIG NO_DEFAULT_PATH PATHS ${ASSIMP_INSTALL_PREFIX})
find_package(ghc_filesystem REQUIRED CONFIG NO_DEFAULT_PATH PATHS ${FILESYSTEM_INSTALL_PREFIX})
find_package(pugixml REQUIRED CONFIG NO_DEFAULT_PATH PATHS ${PUGIXML_INSTALL_PREFIX})
find_package(zlib REQUIRED CONFIG NO_DEFAULT_PATH PATHS ${ZLIB_INSTALL_PREFIX})

list(APPEND CMAKE_PREFIX_PATH ${PNG_INSTALL_PREFIX} ${JPEG_INSTALL_PREFIX})
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)

add_library(CImg::CImg INTERFACE IMPORTED)
set_target_properties(CImg::CImg PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${CIMG_INCLUDE_FILES_PREFIX}
    INTERFACE_LINK_LIBRARIES "PNG::PNG;JPEG::JPEG"
)

# Install OpenGeode-IO third-parties
if(NOT BUILD_SHARED_LIBS)
    install(
        DIRECTORY
            ${ASSIMP_INSTALL_PREFIX}/
            ${JPEG_INSTALL_PREFIX}/
            ${PNG_INSTALL_PREFIX}/
            ${PUGIXML_INSTALL_PREFIX}/
            ${ZLIB_INSTALL_PREFIX}/
        DESTINATION
            .
    )
endif()

# ------------------------------------------------------------------------------------------------
# Configure the OpenGeode-IO libraries
add_subdirectory(src/geode)

# ------------------------------------------------------------------------------------------------
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

# ------------------------------------------------------------------------------------------------
# Configure CPack
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

# This must always be last!
include(CPack)
