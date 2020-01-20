find_package(pybind11 QUIET)
if (NOT pybind11_FOUND)
    execute_process(
        COMMAND git clone --branch v2.4.3 https://github.com/pybind/pybind11 ${CMAKE_CURRENT_BINARY_DIR}/pybind11
        )
    add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/pybind11 ${CMAKE_CURRENT_BINARY_DIR}/build_pybind11)
endif()

if(MINGW)
    add_definitions(-D_hypot=hypot -DMS_WIN64)
endif()
