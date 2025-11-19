set(CMAKE_FOLDER "Tests")



add_executable(TbxMessagingTests)



target_precompile_headers(TbxMessagingTests PRIVATE "pch.h")



file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS

    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"

)

target_sources(TbxMessagingTests PRIVATE ${TEST_SOURCES})

target_include_directories(TbxMessagingTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")



set_target_properties(TbxMessagingTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)



target_link_libraries(TbxMessagingTests PRIVATE

    Tbx::Messaging

    gtest

    gtest_main

    gmock

)



add_test(NAME TbxMessagingTests COMMAND $<TARGET_FILE:TbxMessagingTests>)

