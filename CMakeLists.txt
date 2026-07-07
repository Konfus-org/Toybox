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

# Third-party dependencies are vendored with CMake FetchContent. There is no central
# dependency list: the engine core and each plugin declare, configure, and make available
# the dependencies they own, guarded by `if(NOT TARGET ...)` so a dependency shared by
# several consumers is fetched once (first declaration wins). Only the global FetchContent
# settings live here. Sources are cached under thirdparty/_deps (git-ignored) and shared
# across build directories instead of re-downloaded per preset. (UPDATES_DISCONNECTED is
# intentionally left at its default so that bumping a pinned GIT_TAG is always re-applied.)
if(NOT DEFINED FETCHCONTENT_BASE_DIR)
    # The cache is segmented per generator: FetchContent's sub-build trees are tied to the
    # generator that created them, and CMake hard-errors when a consumer with a different
    # generator (e.g. Studio's Visual Studio preset vs a Ninja engine build) reuses them.
    string(MAKE_C_IDENTIFIER "${CMAKE_GENERATOR}" TBX_FETCHCONTENT_GENERATOR_DIR)
    set(FETCHCONTENT_BASE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/_deps/${TBX_FETCHCONTENT_GENERATOR_DIR}" CACHE PATH
        "Location for FetchContent-vendored third-party sources")
endif()
include(FetchContent)

# Includes and subdirectories
add_subdirectory(resources)
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
