cmake_minimum_required(VERSION 3.6)
project(CDF++ CXX)

include(GNUInstallDirs)

OPTION (CPPCHECK "Analyzes the source code with cppcheck" OFF)
OPTION (CLANG_TIDY "Analyzes the source code with Clang Tidy" OFF)
OPTION (IWYU "Analyzes the source code with Include What You Use" OFF)

set(CMAKE_CXX_STANDARD 17)

if(NOT DEFINED CMAKE_INSTALL_RPATH_USE_LINK_PATH)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()
if(NOT DEFINED CMAKE_MACOSX_RPATH)
  set(CMAKE_MACOSX_RPATH TRUE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

IF(CPPCHECK)
    set(CMAKE_CXX_CPPCHECK "cppcheck;--enable=warning,style")
ENDIF(CPPCHECK)

IF(CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-style=file;-checks=*")
ENDIF(CLANG_TIDY)

IF(IWYU)
    set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "include-what-you-use")
ENDIF(IWYU)

include (TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
 set(CDFpp_BIG_ENDIAN ON)
else()
 set(CDFpp_LITTLE_ENDIAN ON)
endif()

configure_file(config.h.in config.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

find_package(ZLIB REQUIRED)


add_library(CDFpp INTERFACE)

target_include_directories(CDFpp
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(CDFpp INTERFACE ZLIB::ZLIB)


if(MSVC)
  target_compile_options(CDFpp INTERFACE /W4 /WX)
else()
  target_compile_options(CDFpp INTERFACE -Wall -Wextra -pedantic -Wno-strict-aliasing -Werror)
endif()


enable_testing()
    
option(with_pybind11 "Enable Pybind11 wrapper" ON)
if (with_pybind11)
    find_package(pybind11 REQUIRED)
    pybind11_add_module(pycdfpp wrapper/pycdfpp.cpp)
    target_link_libraries(pycdfpp PRIVATE CDFpp)
    install(TARGETS pycdfpp LIBRARY DESTINATION pycdfpp)
endif()

option(with_tests "Enable tests" ON)
if (with_tests)
    add_executable(simple_open tests/simple_open/main.cpp)
    target_link_libraries(simple_open PRIVATE CDFpp)
    target_include_directories(simple_open PRIVATE catch2)

    target_compile_definitions(simple_open PRIVATE DATA_PATH="${CMAKE_CURRENT_LIST_DIR}/tests/resources")
    add_test(test-simple_open simple_open)

    add_executable(endianness tests/endianness/main.cpp)
    target_link_libraries(endianness PUBLIC CDFpp)
    target_include_directories(endianness PRIVATE catch2)
    add_test(test-endianness endianness)

    add_executable(zlib_test tests/zlib/main.cpp)
    target_link_libraries(zlib_test PUBLIC CDFpp)
    target_include_directories(zlib_test PRIVATE catch2)
    add_test(test-zlib_test zlib_test)
endif()



