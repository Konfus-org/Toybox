add_executable(doom_clone doom_clone.cpp)
target_link_libraries(doom_clone PRIVATE tbx)
tbx_copy_runtime_dlls(doom_clone)
