cmake_minimum_required(VERSION 3.21)

project(stbimg LANGUAGES CXX)

add_library(stbimg STATIC)

# Match your Premake globs (add CONFIGURE_DEPENDS so edits retrigger cmake)
file(GLOB_RECURSE STBIMG_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE STBIMG_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE STBIMG_DOCS    CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/**/*.md")

target_sources(stbimg PRIVATE ${STBIMG_SOURCES} ${STBIMG_HEADERS} ${STBIMG_DOCS})

target_include_directories(stbimg
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# C++17 like cppdialect "C++17"
set_target_properties(stbimg PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# Defines like Premake `defines { "STB_IMAGE_IMPLEMENTATION" }`
target_compile_definitions(stbimg PRIVATE STB_IMAGE_IMPLEMENTATION)