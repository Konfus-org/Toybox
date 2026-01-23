tbx_add_module(PluginApi)

tbx_register_module(PluginApi)

target_compile_definitions(PluginApi
    PUBLIC
        TBX_PLUGIN_ABI_VERSION=${TBX_PLUGIN_ABI_VERSION}
)

target_link_libraries(PluginApi
    PUBLIC
        Tbx::Messaging
        Tbx::Debugging
        Tbx::Files
    PRIVATE
        nlohmann_json
)
