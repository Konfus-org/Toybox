set(CMAKE_FOLDER "Tests")



add_executable(TbxPluginApiTests)



target_precompile_headers(TbxPluginApiTests PRIVATE "pch.h")



file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS

    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"

)

target_sources(TbxPluginApiTests PRIVATE ${TEST_SOURCES})

target_include_directories(TbxPluginApiTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")



set_target_properties(TbxPluginApiTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)



target_link_libraries(TbxPluginApiTests PRIVATE

    Tbx::PluginApi

    Tbx::App

    gtest

    gtest_main

    gmock

)



add_test(NAME TbxPluginApiTests COMMAND $<TARGET_FILE:TbxPluginApiTests>)

