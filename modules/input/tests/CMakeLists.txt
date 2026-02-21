set(CMAKE_FOLDER "tests")
include(TbxTestOutput)
add_executable(TbxInputTests)
tbx_set_test_output(TbxInputTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxInputTests PRIVATE "pch.h")
target_sources(TbxInputTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxInputTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxInputTests PRIVATE
    Tbx::Input
    Tbx::Messaging
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxInputTests COMMAND $<TARGET_FILE:TbxInputTests>)
unset(CMAKE_FOLDER)
