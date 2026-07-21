add_executable(tbx_run tbx_run.cpp)
target_link_libraries(tbx_run PRIVATE tbx)
tbx_copy_runtime_dlls(tbx_run)
