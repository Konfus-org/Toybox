set(CMAKE_FOLDER "Tests")

add_executable(TbxCommonTests)

target_precompile_headers(TbxCommonTests PRIVATE "pch.h")

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

list(FILTER TEST_SOURCES EXCLUDE REGEX "string_path_operations_tests\\.cpp$")

if(TEST_SOURCES)
    target_sources(TbxCommonTests PRIVATE ${TEST_SOURCES})
endif()

target_include_directories(TbxCommonTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

set_target_properties(TbxCommonTests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_link_libraries(TbxCommonTests PRIVATE
    Tbx::Common
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxCommonTests COMMAND $<TARGET_FILE:TbxCommonTests>)

set(CMAKE_FOLDER "")
