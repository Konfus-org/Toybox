set(plugin_name AssimpModelLoaderPlugin)

add_library(${plugin_name} SHARED)
add_library(Tbx::Plugins::${plugin_name} ALIAS ${plugin_name})

target_compile_definitions(${plugin_name} PRIVATE TBX_PLUGIN_EXPORTING_SYMBOLS)
target_link_libraries(${plugin_name} PRIVATE
    Tbx::App
    Tbx::Assets
    Tbx::Files
    Tbx::Graphics
    assimp::assimp
)

tbx_register_plugin(
    TARGET ${plugin_name}
    CLASS assimp_model_loader::AssimpModelLoaderPlugin
    HEADER "assimp_model_loader_plugin.h"
    NAME "${plugin_name}"
    VERSION "1.0.0"
    CATEGORY "default"
)
