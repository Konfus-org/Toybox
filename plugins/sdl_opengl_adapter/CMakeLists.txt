add_library(SdlOpenGlAdapterPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(SdlOpenGlAdapterPlugin PRIVATE ${SRCS})
target_include_directories(SdlOpenGlAdapterPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SdlOpenGlAdapterPlugin PRIVATE
    Tbx::App
    Tbx::Graphics
    SDL3::SDL3-shared
    glad::glad
)

tbx_register_plugin(
    TARGET SdlOpenGlAdapterPlugin
    CLASS tbx::plugins::SdlOpenGlAdapterPlugin
    HEADER "sdl_opengl_adapter_plugin.h"
    NAME "SdlOpenGlAdapterPlugin"
    VERSION "1.0.0"
    CATEGORY "rendering"
)

add_library(Tbx::Plugins::SdlOpenGlAdapterPlugin ALIAS SdlOpenGlAdapterPlugin)
