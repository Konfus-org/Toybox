message(STATUS "")
message(STATUS "Configuring Toybox Plugins:")
include(code_gen_utility)

set(CMAKE_FOLDER "plugins")

add_subdirectory(stb_image_loader)
add_subdirectory(tbx_shader_include_loader)
add_subdirectory(assimp_model_loader)
add_subdirectory(tbx_performance_monitor)
add_subdirectory(sdl_base_systems)
add_subdirectory(sdl_opengl_context_manager)
add_subdirectory(sdl_windowing)
add_subdirectory(sdl_input)
add_subdirectory(jolt_physics)
add_subdirectory(opengl_rendering)

unset(CMAKE_FOLDER)
