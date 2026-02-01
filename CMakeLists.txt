cmake_minimum_required(VERSION 4.1.0)
project(Toybox LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TbxPlugins)
include(TbxModules)

# Global configs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT DEFINED TBX_ENABLE_ASSERTS)
  set(TBX_ENABLE_ASSERTS ON CACHE BOOL "Enable runtime asserts in debug builds")
endif()

add_compile_definitions(
  $<$<CONFIG:Debug>:TBX_DEBUG>
  $<$<CONFIG:Release>:TBX_RELEASE>
  $<$<AND:$<CONFIG:Debug>,$<BOOL:TBX_ENABLE_ASSERTS>>:TBX_ASSERTS_ENABLED>
)
enable_testing()

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
