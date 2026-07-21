add_executable(toom toom.cpp)
target_link_libraries(toom PRIVATE tbx)
tbx_copy_runtime_dlls(toom)
target_compile_definitions(toom PRIVATE SAMPLE_ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets")
