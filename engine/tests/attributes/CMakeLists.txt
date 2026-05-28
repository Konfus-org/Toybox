set(CMAKE_FOLDER "tests")
include(test_output)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(TbxAttributeTests)
tbx_set_test_output(TbxAttributeTests)

set(attribute_fixture "${CMAKE_CURRENT_SOURCE_DIR}/attribute_fixtures.h")
set(attribute_generated_root "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(attribute_generated_header "${attribute_generated_root}/attribute_fixtures.generated.h")
set(attribute_generated_source "${attribute_generated_root}/attribute_fixtures.generated.cpp")

add_custom_command(
    OUTPUT
        "${attribute_generated_header}"
        "${attribute_generated_source}"
    COMMAND ${Python3_EXECUTABLE}
        "${PROJECT_SOURCE_DIR}/codegen/tbx_attribute_codegen.py"
        --input "${attribute_fixture}"
        --output-dir "${attribute_generated_root}"
    DEPENDS
        "${PROJECT_SOURCE_DIR}/codegen/tbx_attribute_codegen.py"
        "${PROJECT_SOURCE_DIR}/codegen/asset_codegen.py"
        "${PROJECT_SOURCE_DIR}/codegen/common_codegen.py"
        "${PROJECT_SOURCE_DIR}/codegen/enum_codegen.py"
        "${PROJECT_SOURCE_DIR}/codegen/formatter_codegen.py"
        "${PROJECT_SOURCE_DIR}/codegen/generator.py"
        "${PROJECT_SOURCE_DIR}/codegen/hash_codegen.py"
        "${PROJECT_SOURCE_DIR}/codegen/model.py"
        "${PROJECT_SOURCE_DIR}/codegen/parser.py"
        "${PROJECT_SOURCE_DIR}/codegen/struct_codegen.py"
        "${PROJECT_SOURCE_DIR}/codegen/tests.py"
        "${PROJECT_SOURCE_DIR}/codegen/variant_codegen.py"
        "${attribute_fixture}"
    COMMENT "Generating Toybox attribute test fixtures"
    VERBATIM
)

add_custom_target(
    TbxAttributeCodegen
    DEPENDS
        "${attribute_generated_header}"
        "${attribute_generated_source}")
add_dependencies(TbxAttributeTests TbxAttributeCodegen)

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(FILTER TEST_SOURCES EXCLUDE REGEX "/generated/")

target_precompile_headers(TbxAttributeTests PRIVATE "../pch.h")
target_sources(
    TbxAttributeTests
    PRIVATE
        ${TEST_SOURCES}
        "${attribute_generated_header}"
        "${attribute_generated_source}")
target_include_directories(TbxAttributeTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_options(TbxAttributeTests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/wd5030>
    $<$<CXX_COMPILER_ID:Clang>:-Wno-unknown-attributes>
    $<$<CXX_COMPILER_ID:GNU>:-Wno-attributes>
)

target_link_libraries(TbxAttributeTests PRIVATE
    Tbx::Engine
    gtest
    gtest_main
    gmock
)

add_test(NAME TbxAttributeTests COMMAND $<TARGET_FILE:TbxAttributeTests>)
add_test(
    NAME TbxAttributeCodegenPythonTests
    COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/codegen/tbx_attribute_codegen.py" --self-test
)

unset(CMAKE_FOLDER)
