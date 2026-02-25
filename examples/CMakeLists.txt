set(CMAKE_FOLDER "examples")

include(TbxDefaultIncludes)
tbx_define_default_includes()

add_subdirectory(common)
add_subdirectory(ecs_example)
add_subdirectory(asset_example)
add_subdirectory(input_example)
add_subdirectory(physics_example)
add_subdirectory(lighting_example)

unset(CMAKE_FOLDER)
