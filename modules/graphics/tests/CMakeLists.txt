set(CMAKE_FOLDER "Tests")

add_executable(TbxGraphicsTests)

target_precompile_headers(TbxGraphicsTests PRIVATE "PCH.h")

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_sources(TbxGraphicsTests PRIVATE ${TEST_SOURCES})

target_include_directories(TbxGraphicsTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(TbxGraphicsTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

target_link_libraries(TbxGraphicsTests PRIVATE
    Tbx::Graphics
    Tbx::Math
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxGraphicsTests COMMAND $<TARGET_FILE:TbxGraphicsTests>)
