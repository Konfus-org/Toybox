cmake_minimum_required(VERSION 3.26)

message(STATUS "")
message(STATUS "Configuring Toybox Plugins:")
include(TbxRegisterPlugin)

set(CMAKE_FOLDER "plugins")

add_subdirectory(spd_logger)
add_subdirectory(stb_image_loader)
add_subdirectory(glsl_shader_loader)
add_subdirectory(mat_material_loader)
add_subdirectory(assimp_model_loader)
add_subdirectory(sdl_adapter)
add_subdirectory(sdl_windowing)
add_subdirectory(opengl_rendering)

unset(CMAKE_FOLDER)
message(STATUS "")

