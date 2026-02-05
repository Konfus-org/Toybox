cmake_minimum_required(VERSION 4.1.0)
project(Toybox LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Cache options touched by presets to avoid "unused" warnings.
option(TBX_ENABLE_ASSERTS "Enable Toybox asserts (Debug only)." OFF)
if(DEFINED CTEST_TEST_TIMEOUT)
    set(_TBX_PRESET_CTEST_TEST_TIMEOUT "${CTEST_TEST_TIMEOUT}")
endif()
if(DEFINED CMAKE_MODULE_LINKER_FLAGS_DEBUG)
    set(_TBX_PRESET_MODULE_LINKER_FLAGS_DEBUG "${CMAKE_MODULE_LINKER_FLAGS_DEBUG}")
endif()

# Global configs
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
enable_testing()

# Libs and projects
add_subdirectory(modules)
add_subdirectory(thirdparty)
add_subdirectory(plugins)
add_subdirectory(examples)

# Add utilities
set(CMAKE_FOLDER "utility")
if(WIN32)
    add_custom_target(CompileCommandsLink ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
        BYPRODUCTS "${CMAKE_SOURCE_DIR}/compile_commands.json"
        COMMENT "Copy compile_commands.json into the repo root for clangd."
    )
else()
    add_custom_target(CompileCommandsLink ALL
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
        BYPRODUCTS "${CMAKE_SOURCE_DIR}/compile_commands.json"
        COMMENT "Link compile_commands.json into the repo root for clangd."
    )
endif()
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
