set(CMAKE_FOLDER "Tests")



add_executable(TbxAsyncTests)



target_precompile_headers(TbxAsyncTests PRIVATE "pch.h")



file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS

    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"

)

target_sources(TbxAsyncTests PRIVATE ${TEST_SOURCES})

target_include_directories(TbxAsyncTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")



set_target_properties(TbxAsyncTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)



target_link_libraries(TbxAsyncTests PRIVATE

    Tbx::Async

    gtest

    gtest_main

    gmock

)



add_test(NAME TbxAsyncTests COMMAND $<TARGET_FILE:TbxAsyncTests>)

