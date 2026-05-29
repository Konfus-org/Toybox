set(CMAKE_FOLDER "tests")
include(test_output)

add_executable(TbxScriptingTests)
tbx_set_test_output(TbxScriptingTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxScriptingTests PRIVATE "../pch.h")
target_sources(TbxScriptingTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxScriptingTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxScriptingTests PRIVATE
    Tbx::Engine
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxScriptingTests COMMAND $<TARGET_FILE:TbxScriptingTests>)

unset(CMAKE_FOLDER)
