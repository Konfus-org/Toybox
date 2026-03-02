message(STATUS "")
message(STATUS "Configuring Toybox Plugins:")
include(TbxRegisterPlugin)

set(CMAKE_FOLDER "plugins")

option(TBX_BUILD_PLUGIN_TESTS "Build plugin tests" OFF)

add_subdirectory(stb_image_loader)
add_subdirectory(glsl_shader_loader)
add_subdirectory(mat_material_loader)
add_subdirectory(assimp_model_loader)
add_subdirectory(sdl_base_systems)
add_subdirectory(sdl_opengl_adapter)
add_subdirectory(sdl_windowing)
add_subdirectory(sdl_input)
add_subdirectory(jolt_physics)
add_library(OpenGlRenderingPlugin INTERFACE)
add_library(Tbx::Plugins::OpenGlRenderingPlugin ALIAS OpenGlRenderingPlugin)
if(TBX_BUILD_PLUGIN_TESTS)
    add_subdirectory(tests)
endif()

unset(CMAKE_FOLDER)
