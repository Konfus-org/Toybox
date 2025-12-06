set(CMAKE_FOLDER "tests")
add_executable(TbxAppTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxAppTests PRIVATE "pch.h")
target_sources(TbxAppTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxAppTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(TbxAppTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)

target_link_libraries(TbxAppTests PRIVATE
    Tbx::App
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxAppTests COMMAND $<TARGET_FILE:TbxAppTests>)
unset(CMAKE_FOLDER)
