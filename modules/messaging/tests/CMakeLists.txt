set(CMAKE_FOLDER "tests")
include(TbxTestOutput)
add_executable(TbxMessagingTests)
tbx_set_test_output(TbxMessagingTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxMessagingTests PRIVATE "pch.h")
target_sources(TbxMessagingTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxMessagingTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxMessagingTests PRIVATE
    Tbx::Messaging
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxMessagingTests COMMAND $<TARGET_FILE:TbxMessagingTests>)
unset(CMAKE_FOLDER)
