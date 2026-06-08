include(engine_test_targets)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

tbx_add_engine_test(
    NAME TbxAttributeTests
    ATTRIBUTE_CODEGEN
)

add_test(
    NAME TbxAttributeCodegenPythonTests
    COMMAND ${Python3_EXECUTABLE} "${TBX_CODEGEN_ROOT}/tbx_attribute_codegen.py" --self-test
)
