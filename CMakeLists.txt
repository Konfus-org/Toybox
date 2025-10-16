cmake_minimum_required(VERSION 3.26)
project(Toybox LANGUAGES C CXX)

# Set global C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Defaults, only if the user hasn't set them already.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/bin"
    CACHE PATH "Where to place executables")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/lib"
    CACHE PATH "Where to place shared libraries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/lib"
    CACHE PATH "Where to place static libraries")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/pdb"
    CACHE PATH "Where to place PDBs")

# Global configs
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;ReleaseAssert" CACHE STRING "" FORCE)

# if(MSVC)
#     set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
#     add_compile_options(/MP)
# else()
#     set(BUILD_SHARED_LIBS ON)
#     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
# endif()

# Provide flag variables for the custom "ReleaseAssert" configuration.
foreach(_var
    CMAKE_C_FLAGS_RELEASE
    CMAKE_CXX_FLAGS_RELEASE
    CMAKE_EXE_LINKER_FLAGS_RELEASE
    CMAKE_SHARED_LINKER_FLAGS_RELEASE
    CMAKE_STATIC_LINKER_FLAGS_RELEASE
    CMAKE_MODULE_LINKER_FLAGS_RELEASE)
  string(REPLACE "_RELEASE" "_RELEASEASSERT" _var_name "${_var}")
  set(${_var_name} "${${_var}}" CACHE STRING "" FORCE)
endforeach()

# Core projects
add_subdirectory(Engine)
add_subdirectory(Engine/Tests)
add_subdirectory(Launcher)
target_link_libraries(Engine)
target_link_libraries(Tests)
target_link_libraries(Launcher)

# Dependencies
set(CMAKE_FOLDER "3rd Party")
add_subdirectory(Dependencies)

# Examples
set(CMAKE_FOLDER "Examples")
add_subdirectory(Examples/3dDemo/Runtime)
add_subdirectory(Examples/3dDemo/Launcher)

# Plugins:
set(CMAKE_FOLDER "Plugins")
add_subdirectory(Plugins/Toybox-ImGui-ImmediateUI-Plugin)
add_subdirectory(Plugins/Toybox-JIMS-AssetLoader-Plugin)
add_subdirectory(Plugins/Toybox-OpenGL-Rendering-Plugin)
add_subdirectory(Plugins/Toybox-SDL-Plugins)
add_subdirectory(Plugins/Toybox-Spd-Logging-Plugin)