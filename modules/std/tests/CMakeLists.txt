set(CMAKE_FOLDER "Tests")



add_executable(TbxStdTests)



target_precompile_headers(TbxStdTests PRIVATE "pch.h")



file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS

    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"

)

list(FILTER TEST_SOURCES EXCLUDE REGEX "string_path_operations_tests\\.cpp$")

target_sources(TbxStdTests PRIVATE ${TEST_SOURCES})

target_include_directories(TbxStdTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")



set_target_properties(TbxStdTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)



target_link_libraries(TbxStdTests PRIVATE

    Tbx::Std

    gtest

    gtest_main

    gmock

)



add_test(NAME TbxStdTests COMMAND $<TARGET_FILE:TbxStdTests>)

