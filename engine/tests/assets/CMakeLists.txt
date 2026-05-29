include(engine_test_targets)

tbx_add_engine_test(
    NAME TbxAssetsTests
    ATTRIBUTE_CODEGEN
    WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
    EXTRA_INCLUDES
        "${PROJECT_SOURCE_DIR}/engine/tests/files/include"
)