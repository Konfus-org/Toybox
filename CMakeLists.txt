cmake_minimum_required(VERSION 3.26)
project(Toybox LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TbxPlugins)

# Set global C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Defaults, only if the user hasn't set them already.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;" CACHE STRING "Supported configuration types")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake/build/bin" CACHE PATH "Where to place executables")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake/build/lib" CACHE PATH "Where to place shared libraries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake/build/lib" CACHE PATH "Where to place static libraries")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/cmake/build/pdb" CACHE PATH "Where to place PDBs")

# Global configs
enable_testing()
if(MSVC)
  set_property(GLOBAL PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  add_compile_options("/Zc:preprocessor")
  add_compile_options("/ZI")
  add_compile_options("/MP")
  add_compile_options("/GR")
  add_link_options("/INCREMENTAL")
endif()

# Libs and projects
add_subdirectory(engine)
add_subdirectory(engine/tests)
add_subdirectory(thirdparty)
add_subdirectory(plugins)
add_subdirectory(examples)
unset(CMAKE_FOLDER)
