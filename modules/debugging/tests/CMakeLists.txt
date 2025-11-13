set(CMAKE_FOLDER "Tests")



add_executable(TbxDebuggingTests)



target_precompile_headers(TbxDebuggingTests PRIVATE "pch.h")



file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS

    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"

)

target_sources(TbxDebuggingTests PRIVATE ${TEST_SOURCES})

target_include_directories(TbxDebuggingTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")



set_target_properties(TbxDebuggingTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)



target_link_libraries(TbxDebuggingTests PRIVATE

    Tbx::Debugging

    gtest

    gtest_main

    gmock

)



add_test(NAME TbxDebuggingTests COMMAND $<TARGET_FILE:TbxDebuggingTests>)

