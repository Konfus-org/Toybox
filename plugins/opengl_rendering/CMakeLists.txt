add_library(OpenGlRenderingPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(OpenGlRenderingPlugin PRIVATE ${SRCS})
target_include_directories(OpenGlRenderingPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(OpenGlRenderingPlugin PRIVATE
    Tbx::App
    Tbx::Graphics
    glad::glad
)

if(TARGET SDL3::SDL3-shared)
    target_link_libraries(OpenGlRenderingPlugin PRIVATE
        SDL3::SDL3-shared
    )
    target_compile_definitions(OpenGlRenderingPlugin PRIVATE USING_SDL3)
endif()

tbx_register_plugin(
    TARGET OpenGlRenderingPlugin
    CLASS tbx::plugins::OpenGlRenderingPlugin
    HEADER "opengl_rendering_plugin.h"
    NAME "OpenGlRenderingPlugin"
    VERSION "1.0.0"
    CATEGORY "rendering"
)

add_library(Tbx::Plugins::OpenGlRenderingPlugin ALIAS OpenGlRenderingPlugin)
