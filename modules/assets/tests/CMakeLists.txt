set(CMAKE_FOLDER "tests")
add_executable(TbxAssetsTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxAssetsTests PRIVATE "pch.h")
target_sources(TbxAssetsTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxAssetsTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxAssetsTests PRIVATE
    Tbx::Assets
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxAssetsTests COMMAND $<TARGET_FILE:TbxAssetsTests>)
unset(CMAKE_FOLDER)
