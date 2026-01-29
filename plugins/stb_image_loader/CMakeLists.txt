add_library(StbImageLoaderPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(StbImageLoaderPlugin PRIVATE ${SRCS})
target_include_directories(StbImageLoaderPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(StbImageLoaderPlugin PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
    stbimg
)

tbx_register_plugin(
    TARGET StbImageLoaderPlugin
    CLASS tbx::plugins::StbImageLoaderPlugin
    HEADER "stb_image_loader_plugin.h"
    NAME "StbImageLoaderPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

add_library(Tbx::Plugins::StbImageLoaderPlugin ALIAS StbImageLoaderPlugin)
