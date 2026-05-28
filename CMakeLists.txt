cmake_minimum_required(VERSION 3.28.3)
project(Toybox LANGUAGES C CXX)

# Global configs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


option(TBX_ENABLE_COMPILER_CACHE "Use compiler cache launchers when available" ON)
option(TBX_BUILD_TESTS "Build Toybox test targets" OFF)
option(TBX_FULL_RELEASE "Disable development-only runtime behavior for final release builds" OFF)

if(TBX_FULL_RELEASE)
    add_compile_definitions(TBX_FULL_RELEASE)
endif()

function(tbx_log_cache_options)
    get_cmake_property(cache_variables CACHE_VARIABLES)
    list(FILTER cache_variables INCLUDE REGEX "^TBX_")
    list(SORT cache_variables)

    message(STATUS "Tbx: cache options:")
    foreach(cache_variable IN LISTS cache_variables)
        get_property(cache_value CACHE "${cache_variable}" PROPERTY VALUE)
        message(STATUS "  ${cache_variable}=${cache_value}")
    endforeach()
endfunction()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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
    set(tbx_enable_compiler_cache_launcher ON)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT TBX_CLANG_SCAN_DEPS_PROGRAM)
        message(STATUS "Tbx: skipping compiler cache launcher for Clang because clang-scan-deps was not found")
        set(tbx_enable_compiler_cache_launcher OFF)
    endif()

    if(CMAKE_C_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER)
        message(STATUS "Tbx: using user-provided compiler cache launcher")
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

tbx_log_cache_options()

# Includes and subdirectories
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
add_subdirectory(resources)
add_subdirectory(thirdparty)
add_subdirectory(engine)
add_subdirectory(plugins)
add_subdirectory(examples)

if(TBX_BUILD_TESTS)
    enable_testing()
    set(CMAKE_FOLDER "tests")
    add_subdirectory(engine/tests)
    unset(CMAKE_FOLDER)
endif()

include(utilities)
