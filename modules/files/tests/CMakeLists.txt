set(CMAKE_FOLDER "tests")
add_executable(TbxFileSystemTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxFileSystemTests PRIVATE "pch.h")
target_sources(TbxFileSystemTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxFileSystemTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxFileSystemTests PRIVATE
    Tbx::Files
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxFileSystemTests COMMAND $<TARGET_FILE:TbxFileSystemTests>)
