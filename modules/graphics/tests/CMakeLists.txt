set(CMAKE_FOLDER "tests")
include(TbxTestOutput)

add_executable(TbxGraphicsTests)
tbx_set_test_output(TbxGraphicsTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxGraphicsTests PRIVATE "PCH.h")
target_sources(TbxGraphicsTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxGraphicsTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxGraphicsTests PRIVATE
    Tbx::Graphics
    Tbx::Math
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxGraphicsTests COMMAND $<TARGET_FILE:TbxGraphicsTests>)
