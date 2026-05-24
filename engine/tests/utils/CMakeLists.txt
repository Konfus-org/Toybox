set(CMAKE_FOLDER "tests")
include(test_output)
add_executable(TbxUtilsTests)
tbx_set_test_output(TbxUtilsTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxUtilsTests PRIVATE "../pch.h")
target_sources(TbxUtilsTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxUtilsTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxUtilsTests PRIVATE
    Tbx::Engine
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxUtilsTests COMMAND $<TARGET_FILE:TbxUtilsTests>)
unset(CMAKE_FOLDER)