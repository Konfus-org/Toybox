cmake_minimum_required(VERSION 3.28.3)
project(Toybox.Engine LANGUAGES C CXX)

# Global configs
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TBX_ENABLE_COMPILER_CACHE "Use compiler cache launchers when available" ON)
option(TBX_BUILD_TESTS "Build Toybox test targets" OFF)
option(TBX_FULL_RELEASE "Disable development-only runtime behavior for final release builds" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake")

if(TBX_FULL_RELEASE)
    add_compile_definitions(TBX_FULL_RELEASE)
endif()

include(cache_options)
include(codegen)

# Includes and subdirectories
add_subdirectory(resources)
add_subdirectory(thirdparty)
add_subdirectory(engine)
add_subdirectory(plugins)
add_subdirectory(launcher)

if(TBX_BUILD_TESTS)
    enable_testing()
    set(CMAKE_FOLDER "tests")
    add_subdirectory(engine/tests)
    unset(CMAKE_FOLDER)
endif()

include(utilities)
