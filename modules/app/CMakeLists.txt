tbx_add_module(App)

tbx_register_module(App)

target_link_libraries(App
    PUBLIC
        Tbx::Time
        Tbx::Math
        Tbx::Messaging
        Tbx::PluginApi
        Tbx::ECS
        Tbx::Debugging
        Tbx::Graphics
)
