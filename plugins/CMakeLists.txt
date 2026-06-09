message(STATUS "")
message(STATUS "Configuring Toybox Plugins:")
include(plugin_targets)
include(default_plugins)

set(CMAKE_FOLDER "plugins")

foreach(plugin_dir IN ITEMS
    stb_image_loader
    tbx_shader_include_loader
    assimp_model_loader
    tbx_performance_monitor
    sdl_base_systems
    sdl_opengl_context_manager
    sdl_windowing
    sdl_input
    jolt_physics
    opengl_rendering
)
    add_subdirectory(${plugin_dir})
endforeach()

tbx_define_default_plugins()

unset(CMAKE_FOLDER)
