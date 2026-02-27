set(plugin_name StbImageLoaderPlugin)

add_library(${plugin_name} SHARED)
add_library(Tbx::Plugins::${plugin_name} ALIAS ${plugin_name})


file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(${plugin_name} PRIVATE ${SRCS})
target_include_directories(${plugin_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(${plugin_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${plugin_name} PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
    stbimg
)

tbx_register_plugin(
    TARGET ${plugin_name}
    CLASS stb_image_loader::StbImageLoaderPlugin
    HEADER "tbx/plugins/stb_image_loader/stb_image_loader_plugin.h"
    NAME "${plugin_name}"
    VERSION "1.0.0"
    CATEGORY "default"
)
