message(STATUS "")
message(STATUS "Configuring Toybox Plugins:")
include(TbxRegisterPlugin)

set(CMAKE_FOLDER "plugins")

add_subdirectory(stb_image_loader)
add_subdirectory(glsl_shader_loader)
add_subdirectory(mat_material_loader)
add_subdirectory(assimp_model_loader)
add_subdirectory(sdl_base_systems)
add_subdirectory(sdl_opengl_adapter)
add_subdirectory(sdl_windowing)
add_subdirectory(sdl_input)
add_subdirectory(opengl_rendering)
add_subdirectory(tests)

unset(CMAKE_FOLDER)
