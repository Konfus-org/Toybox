set(CMAKE_FOLDER "tests")
add_executable(TbxECSTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxECSTests PRIVATE "pch.h")
target_sources(TbxECSTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxECSTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxECSTests PRIVATE
    Tbx::ECS
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxECSTests COMMAND $<TARGET_FILE:TbxECSTests>)
unset(CMAKE_FOLDER)
