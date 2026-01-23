tbx_register_plugin(
    TARGET OpenGLRenderingPlugin
    CLASS tbx::plugins::OpenGLRenderingPlugin
    HEADER "opengl_rendering_plugin.h"
    NAME "OpenGLRenderingPlugin"
    VERSION "1.0.0"
    CATEGORY "rendering"
)

target_link_libraries(OpenGLRenderingPlugin PRIVATE
    Tbx::App
    Tbx::Graphics
)

add_library(Tbx::Plugins::OpenGLRenderingPlugin ALIAS OpenGLRenderingPlugin)
