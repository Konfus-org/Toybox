cmake_minimum_required(VERSION 4.1.0)
project(Toybox LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Global configs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
enable_testing()

# Libs and projects
add_subdirectory(modules)
include(TbxDefaultIncludes)
tbx_define_default_includes()
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
