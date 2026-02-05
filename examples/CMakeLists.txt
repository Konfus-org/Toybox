set(CMAKE_FOLDER "examples")

include(TbxDefaultIncludes)
tbx_define_default_includes()

add_subdirectory(plugin_example)
add_subdirectory(ecs_example)
add_subdirectory(asset_example)

unset(CMAKE_FOLDER)