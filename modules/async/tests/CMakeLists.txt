set(CMAKE_FOLDER "tests")
add_executable(TbxAsyncTests)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxAsyncTests PRIVATE "pch.h") 
target_sources(TbxAsyncTests PRIVATE ${TEST_SOURCES}) 
target_include_directories(TbxAsyncTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(TbxAsyncTests PRIVATE
    Tbx::Async
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxAsyncTests COMMAND $<TARGET_FILE:TbxAsyncTests>)
unset(CMAKE_FOLDER)