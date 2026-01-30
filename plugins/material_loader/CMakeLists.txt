add_library(MaterialLoaderPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(MaterialLoaderPlugin PRIVATE ${SRCS})
target_include_directories(MaterialLoaderPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(MaterialLoaderPlugin PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
)

tbx_register_plugin(
    TARGET MaterialLoaderPlugin
    CLASS tbx::plugins::MaterialLoaderPlugin
    HEADER "material_loader_plugin.h"
    NAME "MaterialLoaderPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

add_library(Tbx::Plugins::MaterialLoaderPlugin ALIAS MaterialLoaderPlugin)
