include(engine_test_targets)

tbx_add_engine_test(
    NAME TbxFileSystemTests
    EXTRA_INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)