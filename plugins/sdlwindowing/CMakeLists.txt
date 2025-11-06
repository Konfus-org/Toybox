add_library(SdlWindowingPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(SdlWindowingPlugin PRIVATE ${SRCS})
target_include_directories(SdlWindowingPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SdlWindowingPlugin PRIVATE
    Engine
    SDL3::SDL3-shared
    Tbx::Plugins::SdlAdapterPlugin
)

tbx_register_plugin(
    TARGET SdlWindowingPlugin
    CLASS tbx::plugins::sdlwindowing::SdlWindowingPlugin
    HEADER "sdl_windowing_plugin.h"
    NAME "SdlWindowingPlugin"
    VERSION "1.0.0"
    TYPE "windowing"
)

add_library(Tbx::Plugins::SdlWindowingPlugin ALIAS SdlWindowingPlugin)
