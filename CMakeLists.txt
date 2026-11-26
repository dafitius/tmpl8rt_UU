cmake_minimum_required(VERSION 3.29)

project(tmpl8rt_UU LANGUAGES C CXX)

if(WIN32)
    set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Keep all build artifacts inside the active build tree.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if(CMAKE_CONFIGURATION_TYPES)
    foreach(cfg ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER "${cfg}" cfg_u)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg_u} "${CMAKE_BINARY_DIR}/bin/${cfg}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg_u} "${CMAKE_BINARY_DIR}/lib/${cfg}")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg_u} "${CMAKE_BINARY_DIR}/lib/${cfg}")
    endforeach()
endif()

add_subdirectory("1. Basics")

