tbx_add_plugin(
    NAME SdlOpenGlContextManagerPlugin
    PRIVATE_LINKS
        EnTT::EnTT
        Tbx::Plugins::SdlBaseSystemsPlugin
        SDL3::SDL3-shared
)