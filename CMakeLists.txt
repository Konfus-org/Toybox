cmake_minimum_required(VERSION 3.28.3)
project(Toybox LANGUAGES C CXX)
enable_testing()

# Global configs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TBX_BUILD_EXAMPLES "Build Toybox examples" ON)

# Includes and subdirectories
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
add_subdirectory(modules)
add_subdirectory(thirdparty)
add_subdirectory(plugins)
if(TBX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(TbxUtilities)
