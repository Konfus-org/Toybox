add_executable(doom_clone doom_clone.cpp)
target_link_libraries(doom_clone PRIVATE tbx)
tbx_copy_runtime_dlls(doom_clone)
target_compile_definitions(doom_clone PRIVATE SAMPLE_ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets")
