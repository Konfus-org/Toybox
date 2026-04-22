cmake_minimum_required(VERSION 3.28.3)
project(Toybox LANGUAGES C CXX)
enable_testing()

# Global configs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


option(TBX_ENABLE_COMPILER_CACHE "Use compiler cache launchers when available" ON)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    find_program(TBX_CLANG_SCAN_DEPS_PROGRAM
        NAMES
            clang-scan-deps
            clang-scan-deps-20
            clang-scan-deps-19
            clang-scan-deps-18
            clang-scan-deps-17
    )

    if(TBX_CLANG_SCAN_DEPS_PROGRAM)
        set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "${TBX_CLANG_SCAN_DEPS_PROGRAM}" CACHE FILEPATH "" FORCE)
        set(CMAKE_C_COMPILER_CLANG_SCAN_DEPS "${TBX_CLANG_SCAN_DEPS_PROGRAM}" CACHE FILEPATH "" FORCE)
    else()
        message(STATUS "Tbx: clang-scan-deps was not found; Clang dependency scanning may fail")
    endif()
endif()

if(TBX_ENABLE_COMPILER_CACHE)
    set(CMAKE_C_COMPILER_LAUNCHER "" CACHE STRING "" FORCE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "" CACHE STRING "" FORCE)
    set(tbx_enable_compiler_cache_launcher ON)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT TBX_CLANG_SCAN_DEPS_PROGRAM)
        message(STATUS "Tbx: skipping compiler cache launcher for Clang because clang-scan-deps was not found")
        set(tbx_enable_compiler_cache_launcher OFF)
    endif()

    if(tbx_enable_compiler_cache_launcher)
        find_program(TBX_COMPILER_CACHE_PROGRAM NAMES sccache ccache)
        if(TBX_COMPILER_CACHE_PROGRAM)
            message(STATUS "Tbx: using compiler cache launcher: ${TBX_COMPILER_CACHE_PROGRAM}")
            set(CMAKE_C_COMPILER_LAUNCHER "${TBX_COMPILER_CACHE_PROGRAM}" CACHE STRING "" FORCE)
            set(CMAKE_CXX_COMPILER_LAUNCHER "${TBX_COMPILER_CACHE_PROGRAM}" CACHE STRING "" FORCE)
        else()
            message(STATUS "Tbx: compiler cache launcher not found (searched: sccache, ccache)")
        endif()
    endif()
endif()

# Includes and subdirectories
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
add_subdirectory(resources)
add_subdirectory(thirdparty)
add_subdirectory(core)
add_subdirectory(plugins)
add_subdirectory(examples)

include(utilities)
