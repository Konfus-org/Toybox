set(CMAKE_FOLDER "Tests")



add_executable(TbxTimeTests)



target_precompile_headers(TbxTimeTests PRIVATE "pch.h")



file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS

    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"

)

target_sources(TbxTimeTests PRIVATE ${TEST_SOURCES})

target_include_directories(TbxTimeTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")



set_target_properties(TbxTimeTests PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)



target_link_libraries(TbxTimeTests PRIVATE

    Tbx::Time

    gtest

    gtest_main

    gmock

)



add_test(NAME TbxTimeTests COMMAND $<TARGET_FILE:TbxTimeTests>)

