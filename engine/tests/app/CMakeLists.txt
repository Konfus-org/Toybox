include(engine_test_targets)

tbx_add_engine_test(
    NAME TbxAppTests
    EXTRA_INCLUDES
        "${PROJECT_SOURCE_DIR}/engine/tests/files/include"
        "${PROJECT_SOURCE_DIR}/plugins/tbx_performance_monitor/src"
    EXTRA_LIBRARIES
        PerformanceMonitor
)
