include(engine_test_targets)

tbx_add_engine_test(
    NAME TbxAppTests
    EXTRA_INCLUDES
        "${PROJECT_SOURCE_DIR}/engine/tests/files/include"
)