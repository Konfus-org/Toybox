cmake_minimum_required(VERSION 4.1.0)
project(Toybox LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TbxPlugins)
include(TbxModules)

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

set(TBX_PLUGIN_ABI_VERSION 1 CACHE STRING "Plugin ABI version")

if(NOT DEFINED TBX_ENABLE_ASSERTS)
  set(TBX_ENABLE_ASSERTS ON CACHE BOOL "Enable runtime asserts in debug builds")
endif()

add_compile_definitions(
  $<$<CONFIG:Debug>:TBX_DEBUG>
  $<$<CONFIG:Release>:TBX_RELEASE>
  $<$<AND:$<CONFIG:Debug>,$<BOOL:TBX_ENABLE_ASSERTS>>:TBX_ASSERTS_ENABLED>
)

# Global configs
enable_testing()

if(MSVC)
  add_compile_options("/Zc:preprocessor")
  add_compile_options("/MP")
  add_compile_options("/GR")
  add_link_options("$<$<CONFIG:Debug>:/INCREMENTAL>")
endif()

# Libs and projects
add_subdirectory(modules)
add_subdirectory(thirdparty)
add_subdirectory(plugins)
add_subdirectory(examples)

# Add tests target to run all tests at once
set(CMAKE_FOLDER "tests")
add_custom_target(Tests
    DEPENDS
        TbxCommonTests
        TbxFileSystemTests
        TbxMathTests
        TbxAsyncTests
        TbxTimeTests
        TbxMessagingTests
        TbxPluginApiTests
        TbxECSTests
        TbxGraphicsTests
        TbxAppTests
)
unset(CMAKE_FOLDER)
