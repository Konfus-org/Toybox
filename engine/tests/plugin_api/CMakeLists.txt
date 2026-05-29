include(engine_test_targets)

tbx_add_engine_test(
    NAME TbxPluginApiTests
    EXTRA_INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/engine/tests/files/include"
)