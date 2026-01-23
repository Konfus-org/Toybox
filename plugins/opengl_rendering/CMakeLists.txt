tbx_register_plugin(
    TARGET OpenGLRenderingPlugin
    CLASS tbx::plugins::OpenGlRenderingPlugin
    HEADER "opengl_rendering_plugin.h"
    NAME "OpenGLRenderingPlugin"
    VERSION "1.0.0"
    CATEGORY "rendering"
)

target_link_libraries(OpenGLRenderingPlugin PRIVATE
    Tbx::App
    Tbx::Graphics
    glad::glad
    SDL3::SDL3-shared
)

target_compile_definitions(OpenGLRenderingPlugin PRIVATE
    USING_SDL3
)

add_library(Tbx::Plugins::OpenGLRenderingPlugin ALIAS OpenGLRenderingPlugin)
