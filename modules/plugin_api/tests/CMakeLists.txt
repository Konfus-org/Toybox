set(CMAKE_FOLDER "tests")

add_executable(TbxPluginApiTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxPluginApiTests PRIVATE "pch.h")
target_sources(TbxPluginApiTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxPluginApiTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(TbxPluginApiTests PRIVATE
    Tbx::App
    Tbx::PluginApi
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxPluginApiTests COMMAND $<TARGET_FILE:TbxPluginApiTests>)
unset(CMAKE_FOLDER)