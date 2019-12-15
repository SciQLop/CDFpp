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
 add_compile_definitions(CDFpp_BIG_ENDIAN)
else()
 add_compile_definitions(CDFpp_LITTLE_ENDIAN)
endif()

enable_testing()

#add_subdirectory(sw)

#set(CMAKE_INCLUDE_CURRENT_DIR ON)

#set(SOURCES
#    src/main.c
#    )
    
#option(my_option "My Option" ON)
#if (my_option)
#	add_definitions(-DMY_OPTION)
#endif()


add_executable(simple_open tests/simple_open/main.cpp)
target_include_directories(simple_open PRIVATE include)
target_include_directories(simple_open PRIVATE catch2)

target_compile_definitions(simple_open PRIVATE DATA_PATH="${CMAKE_CURRENT_LIST_DIR}/tests/resources")
add_test(test-simple_open simple_open)

add_executable(endianness tests/endianness/main.cpp)
target_include_directories(endianness PRIVATE include)
target_include_directories(endianness PRIVATE catch2)
add_test(test-endianness endianness)
#add_library(MyLib ${SOURCES})



