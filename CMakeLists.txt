cmake_minimum_required(VERSION 3.20)
project(Toybox LANGUAGES CXX)

# Set global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable testing
enable_testing()

# Global configs
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;ReleaseAssert" CACHE STRING "" FORCE)

add_compile_definitions(
    $<$<CONFIG:Debug>:TBX_DEBUG>
    $<$<CONFIG:Release>:TBX_RELEASE>
    $<$<CONFIG:ReleaseAssert>:TBX_RELEASE_ASSERT>
)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    add_compile_options(/MP)
else()
    set(BUILD_SHARED_LIBS ON)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
endif()

# Dependencies
add_subdirectory(Dependencies/glad/cmake)
add_subdirectory(Dependencies/glm)
add_subdirectory(Dependencies/googletest)
add_subdirectory(Dependencies/ImGui)
add_subdirectory(Dependencies/nlohmann_json)
add_subdirectory(Dependencies/SDL)
add_subdirectory(Dependencies/spdlog)
add_subdirectory(Dependencies/stbimg)
add_subdirectory(Dependencies/sys_info)

# Core projects
add_subdirectory(Engine)
add_subdirectory(Engine/Tests)

# Plugins:
# add_subdirectory(Plugins)

if(MSVC)
    set_property(TARGET Engine PROPERTY FOLDER "Toybox")
    set_property(TARGET Tests PROPERTY FOLDER "Toybox")

    set_property(TARGET glad PROPERTY FOLDER "Dependencies")
    set_property(TARGET glm PROPERTY FOLDER "Dependencies")
    set_property(TARGET gtest PROPERTY FOLDER "Dependencies")
    set_property(TARGET gtest_main PROPERTY FOLDER "Dependencies")
    set_property(TARGET gmock PROPERTY FOLDER "Dependencies")
    set_property(TARGET gmock_main PROPERTY FOLDER "Dependencies")
    set_property(TARGET ImGui PROPERTY FOLDER "Dependencies")
    set_property(TARGET nlohmann_json PROPERTY FOLDER "Dependencies")
    set_property(TARGET SDL PROPERTY FOLDER "Dependencies")
    set_property(TARGET spdlog PROPERTY FOLDER "Dependencies")
    set_property(TARGET stbimg PROPERTY FOLDER "Dependencies")
    set_property(TARGET sys_info PROPERTY FOLDER "Dependencies")

    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")
endif()