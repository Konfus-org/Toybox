cmake_minimum_required(VERSION 3.20)
project(Toybox LANGUAGES CXX)

# Set global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(out_root "${CMAKE_BINARY_DIR}/out")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Get required packages
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Enable testing
enable_testing()

# Global configs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;ReleaseAssert" CACHE STRING "" FORCE)

add_library(tbx_config INTERFACE)
target_compile_definitions(tbx_config INTERFACE
  $<$<PLATFORM_ID:Windows>:TBX_PLATFORM_WINDOWS=1;WIN32_LEAN_AND_MEAN;NOMINMAX>
  $<$<PLATFORM_ID:Darwin>:TBX_PLATFORM_MACOS=1>
  $<$<PLATFORM_ID:Linux>:TBX_PLATFORM_LINUX=1>
  $<$<CONFIG:Debug>:TBX_DEBUG=1>
  $<$<CONFIG:Release>:TBX_RELEASE=1>
  $<$<CONFIG:ReleaseAssert>:TBX_RELEASE_ASSERT=1>
  COMPILING_TOYBOX=1
)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/MP)
else()
    set(BUILD_SHARED_LIBS ON)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
endif()

# Dependency configs
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
set(IMGUI_PLATFORM   SDL3   CACHE STRING "" FORCE) 
set(IMGUI_RENDERER   OpenGL3 CACHE STRING "" FORCE)
set(IMGUI_GL_LOADER  GLAD   CACHE STRING "" FORCE)
set(IMGUI_BUILD_DEMO OFF    CACHE BOOL   "" FORCE)
set(GLAD_ROOT "${CMAKE_SOURCE_DIR}/Dependencies/glad")

# Dependencies
add_subdirectory(Dependencies/glad/cmake glad_cmake)
glad_add_library(glad_gl_core_46 REPRODUCIBLE API gl:core=4.6)
add_subdirectory(Dependencies/glm)
add_subdirectory(Dependencies/googletest)
add_subdirectory(Dependencies/nlohmann_json)
add_subdirectory(Dependencies/SDL)
add_subdirectory(Dependencies/spdlog)
add_subdirectory(Dependencies/stbimg)
add_subdirectory(Dependencies/sys_info)
add_subdirectory(Dependencies/imgui)

# ImGui needs specific configuration depending on backends

# Core projects
add_subdirectory(Engine)
add_subdirectory(Engine/Tests)
add_subdirectory(Launcher)
target_link_libraries(Engine PRIVATE tbx_config)
target_link_libraries(Tests  PRIVATE tbx_config)
target_link_libraries(Launcher  PRIVATE tbx_config)

# Plugins:
# add_subdirectory(Plugins)

if(MSVC)
    set_property(TARGET Engine PROPERTY FOLDER "Toybox")
    set_property(TARGET Tests PROPERTY FOLDER "Toybox")
    set_property(TARGET Launcher PROPERTY FOLDER "Toybox")

    set_property(TARGET glad_gl_core_46 PROPERTY FOLDER "Dependencies")
    set_property(TARGET glm PROPERTY FOLDER "Dependencies")
    set_property(TARGET gtest PROPERTY FOLDER "Dependencies")
    set_property(TARGET gtest_main PROPERTY FOLDER "Dependencies")
    set_property(TARGET gmock PROPERTY FOLDER "Dependencies")
    set_property(TARGET gmock_main PROPERTY FOLDER "Dependencies")
    #set_property(TARGET imgui PROPERTY FOLDER "Dependencies")
    set_property(TARGET nlohmann_json PROPERTY FOLDER "Dependencies")
    set_property(TARGET SDL_uclibc PROPERTY FOLDER "Dependencies")
    set_property(TARGET SDL3-shared PROPERTY FOLDER "Dependencies")
    set_property(TARGET spdlog PROPERTY FOLDER "Dependencies")
    set_property(TARGET stbimg PROPERTY FOLDER "Dependencies")
    set_property(TARGET sys_info PROPERTY FOLDER "Dependencies")
    set_property(TARGET sys_core PROPERTY FOLDER "Dependencies")

    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
endif()