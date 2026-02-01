add_library(SdrShaderLoaderPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(SdrShaderLoaderPlugin PRIVATE ${SRCS})
target_include_directories(SdrShaderLoaderPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(SdrShaderLoaderPlugin PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
)

tbx_register_plugin(
    TARGET SdrShaderLoaderPlugin
    CLASS tbx::plugins::SdrShaderLoaderPlugin
    HEADER "sdr_shader_loader_plugin.h"
    NAME "SdrShaderLoaderPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

add_library(Tbx::Plugins::SdrShaderLoaderPlugin ALIAS SdrShaderLoaderPlugin)
