add_library(ShaderLoaderPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(ShaderLoaderPlugin PRIVATE ${SRCS})
target_include_directories(ShaderLoaderPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(ShaderLoaderPlugin PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
)

tbx_register_plugin(
    TARGET ShaderLoaderPlugin
    CLASS tbx::plugins::ShaderLoaderPlugin
    HEADER "shader_loader_plugin.h"
    NAME "ShaderLoaderPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

add_library(Tbx::Plugins::ShaderLoaderPlugin ALIAS ShaderLoaderPlugin)
