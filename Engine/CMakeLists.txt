cmake_minimum_required(VERSION 3.20)
project(Engine LANGUAGES CXX)

# C++20 and dynamic runtime
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create shared library
add_library(Engine SHARED)

# Precompiled header
target_precompile_headers(Engine PRIVATE "Tbx/PCH.h")

# Source files
file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Include/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.md"
)
target_sources(Engine PRIVATE ${ENGINE_SOURCES})

# Include directories
set(MAIN_SCRIPT_DIR "${CMAKE_SOURCE_DIR}")  # Adjust if needed

target_include_directories(Engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Include"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    "${MAIN_SCRIPT_DIR}/Dependencies/glm"
    "${MAIN_SCRIPT_DIR}/Dependencies/nlohmann_json/include"
)

# Link libraries
target_link_libraries(Engine PRIVATE
    glm
    nlohmann_json
)

# Preprocessor definitions
target_compile_definitions(Engine PRIVATE
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_LEFT_HANDED
    GLM_DEPTH_ZERO_TO_ONE
)

# Platform-specific links
if(WIN32)
    target_link_libraries(Engine PRIVATE DbgHelp.lib)
endif()

