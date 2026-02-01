set(CMAKE_FOLDER "tests")
add_executable(TbxCommonTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxCommonTests PRIVATE "pch.h")
target_sources(TbxCommonTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxCommonTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxCommonTests PRIVATE
    Tbx::Common
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxCommonTests COMMAND $<TARGET_FILE:TbxCommonTests>)
unset(CMAKE_FOLDER)