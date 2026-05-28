set(CMAKE_FOLDER "tests")
include(test_output)
include(code_gen_utility)
add_executable(TbxECSTests)
tbx_set_test_output(TbxECSTests)

tbx_codegen_generate_attribute_headers(
    TARGET TbxECSTests
    SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/generated"
    INCLUDE_SCOPE PRIVATE
)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

target_precompile_headers(TbxECSTests PRIVATE "../pch.h")
target_sources(TbxECSTests PRIVATE ${TEST_SOURCES})
target_include_directories(TbxECSTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(TbxECSTests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/wd5030>
    $<$<CXX_COMPILER_ID:Clang>:-Wno-unknown-attributes>
    $<$<CXX_COMPILER_ID:GNU>:-Wno-attributes>
)

target_link_libraries(TbxECSTests PRIVATE
    Tbx::Engine
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxECSTests COMMAND $<TARGET_FILE:TbxECSTests>)
unset(CMAKE_FOLDER)
