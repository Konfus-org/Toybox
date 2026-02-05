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

if(NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
endif()

if(NOT DEFINED CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
endif()

if(NOT DEFINED CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
endif()
enable_testing()

# Libs and projects
add_subdirectory(modules)
add_subdirectory(thirdparty)
add_subdirectory(plugins)
add_subdirectory(examples)

# Add utilities
set(CMAKE_FOLDER "utility")
set(TBX_COMPILE_COMMANDS_SOURCE "${CMAKE_BINARY_DIR}/compile_commands.json")
set(TBX_COMPILE_COMMANDS_DESTINATION "${CMAKE_SOURCE_DIR}/compile_commands.json")
add_custom_target(CompileCommandsLink ALL
    COMMAND ${CMAKE_COMMAND}
        -D "TBX_COMPILE_COMMANDS_SOURCE=${TBX_COMPILE_COMMANDS_SOURCE}"
        -D "TBX_COMPILE_COMMANDS_DESTINATION=${TBX_COMPILE_COMMANDS_DESTINATION}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyCompileCommands.cmake"
    BYPRODUCTS "${TBX_COMPILE_COMMANDS_DESTINATION}"
    COMMENT "Sync compile_commands.json into the repo root for clangd."
)
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
