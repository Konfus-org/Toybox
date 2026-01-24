add_library(StbImageAssetLoaderPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(StbImageAssetLoaderPlugin PRIVATE ${SRCS})
target_include_directories(StbImageAssetLoaderPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(StbImageAssetLoaderPlugin PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
    stbimg
)

tbx_register_plugin(
    TARGET StbImageAssetLoaderPlugin
    CLASS tbx::plugins::StbImageAssetLoaderPlugin
    HEADER "stb_image_loader_plugin.h"
    NAME "StbImageAssetLoaderPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

add_library(Tbx::Plugins::StbImageAssetLoaderPlugin ALIAS StbImageAssetLoaderPlugin)
