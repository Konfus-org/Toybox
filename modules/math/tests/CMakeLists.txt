add_executable(TbxMathTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxMathTests PRIVATE "PCH.h")
target_sources(TbxMathTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxMathTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxMathTests PRIVATE
    Tbx::Math
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxMathTests COMMAND $<TARGET_FILE:TbxMathTests>)
