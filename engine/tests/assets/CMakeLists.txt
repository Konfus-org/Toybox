set(CMAKE_FOLDER "tests")
include(test_output)
include(code_gen_utility)
add_executable(TbxAssetsTests)
tbx_set_test_output(TbxAssetsTests)

tbx_codegen_generate_attribute_headers(
    TARGET TbxAssetsTests
    SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/generated"
    INCLUDE_SCOPE PRIVATE
)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxAssetsTests PRIVATE "../pch.h")
target_sources(TbxAssetsTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxAssetsTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/engine/tests/files/include"
)

target_compile_options(TbxAssetsTests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/wd5030>
    $<$<CXX_COMPILER_ID:Clang>:-Wno-unknown-attributes>
    $<$<CXX_COMPILER_ID:GNU>:-Wno-attributes>
)

target_link_libraries(TbxAssetsTests PRIVATE
    Tbx::Engine
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxAssetsTests COMMAND $<TARGET_FILE:TbxAssetsTests>)
set_tests_properties(TbxAssetsTests PROPERTIES WORKING_DIRECTORY "${PROJECT_BINARY_DIR}")
unset(CMAKE_FOLDER)
