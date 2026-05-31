tbx_add_example_launcher(
    NAME ThreeDExampleLauncher
    RUNTIME_TARGET Tbx::Examples::ThreeDExampleRuntime
)

add_custom_command(
    TARGET ThreeDExampleLauncher
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets/Settings.json"
        "$<TARGET_FILE_DIR:ThreeDExampleLauncher>/Settings.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets/Settings.json.meta"
        "$<TARGET_FILE_DIR:ThreeDExampleLauncher>/Settings.json.meta"
)
