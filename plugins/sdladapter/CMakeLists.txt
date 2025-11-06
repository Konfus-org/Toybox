add_library(SdlAdapterPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(SdlAdapterPlugin PRIVATE ${SRCS})
target_include_directories(SdlAdapterPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SdlAdapterPlugin PRIVATE
    Tbx::Engine
    SDL3::SDL3
)

tbx_register_plugin(
    TARGET SdlAdapterPlugin
    CLASS tbx::plugins::sdladapter::SdlAdapterPlugin
    HEADER "sdl_adapter_plugin.h"
    NAME "SdlAdapterPlugin"
    VERSION "1.0.0"
    TYPE "system"
)

add_library(Tbx::Plugins::SdlAdapterPlugin ALIAS SdlAdapterPlugin)
