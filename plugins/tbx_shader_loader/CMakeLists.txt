set(plugin_name ShaderIncludeLoader)

add_library(${plugin_name} SHARED)
add_library(Tbx::Plugins::${plugin_name} ALIAS ${plugin_name})

target_include_directories(${plugin_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${plugin_name} PRIVATE
        Tbx::Engine
        EnTT::EnTT
        Tbx::Resources
)

tbx_register_plugin(
        TARGET ${plugin_name}
        CLASS tbx::shader_loader::ShaderIncludeLoader
        HEADER "tbx/plugins/shader_include_loader/shader_include_loader.h"
        NAME "${plugin_name}"
        VERSION "1.0.0"
        CATEGORY "default"
)
