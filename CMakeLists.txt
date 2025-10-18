cmake_minimum_required(VERSION 3.26)
project(Toybox LANGUAGES C CXX)

# Set global C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Defaults, only if the user hasn't set them already.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;" CACHE STRING "Supported configuration types")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/bin" CACHE PATH "Where to place executables")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/lib" CACHE PATH "Where to place shared libraries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/lib" CACHE PATH "Where to place static libraries")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/pdb" CACHE PATH "Where to place PDBs")

# Global configs
if(MSVC)
  # Requires CMP0091 to be set to NEW (default for CMake >= 3.15)
  set_property(GLOBAL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  add_compile_options(/ZI)
  add_compile_options(/MP)
  add_link_options(/INCREMENTAL)
endif()

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
add_subdirectory(Plugins/Toybox-TIMS-AssetLoader-Plugin)
add_subdirectory(Plugins/Toybox-OpenGL-Rendering-Plugin)
add_subdirectory(Plugins/Toybox-SDL-Plugins)
add_subdirectory(Plugins/Toybox-Spd-Logging-Plugin)
unset(CMAKE_FOLDER)