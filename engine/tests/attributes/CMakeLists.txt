include(engine_test_targets)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

tbx_add_engine_test(
    NAME TbxAttributeTests
    ATTRIBUTE_CODEGEN
)

add_test(
    NAME TbxAttributeCodegenPythonTests
    COMMAND ${Python3_EXECUTABLE} "${PROJECT_SOURCE_DIR}/codegen/tbx_attribute_codegen.py" --self-test
)