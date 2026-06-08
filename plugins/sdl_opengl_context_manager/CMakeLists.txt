tbx_add_plugin(
    NAME SdlOpenGlContextManager
    PRIVATE_LINKS
        EnTT::EnTT
        Tbx::Plugins::SdlBaseSystems
        SDL3::SDL3-shared
)
