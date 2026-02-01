add_library(MatMaterialLoaderPlugin SHARED)

file(GLOB_RECURSE SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.*")
target_sources(MatMaterialLoaderPlugin PRIVATE ${SRCS})
target_include_directories(MatMaterialLoaderPlugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(MatMaterialLoaderPlugin PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
)

tbx_register_plugin(
    TARGET MatMaterialLoaderPlugin
    CLASS tbx::plugins::MatMaterialLoaderPlugin
    HEADER "mat_material_loader_plugin.h"
    NAME "MatMaterialLoaderPlugin"
    VERSION "1.0.0"
    CATEGORY "default"
)

add_library(Tbx::Plugins::MatMaterialLoaderPlugin ALIAS MatMaterialLoaderPlugin)
