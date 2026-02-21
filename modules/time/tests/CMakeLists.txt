set(CMAKE_FOLDER "tests")
include(TbxTestOutput)
add_executable(TbxTimeTests)
tbx_set_test_output(TbxTimeTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxTimeTests PRIVATE "pch.h")
target_sources(TbxTimeTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxTimeTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxTimeTests PRIVATE
    Tbx::Time
    Tbx::Messaging
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxTimeTests COMMAND $<TARGET_FILE:TbxTimeTests>)
unset(CMAKE_FOLDER)
