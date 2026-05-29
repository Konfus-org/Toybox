tbx_add_plugin(
    NAME SdlWindowingPlugin
    PRIVATE_LINKS
        EnTT::EnTT
        Tbx::Plugins::SdlBaseSystemsPlugin
        SDL3::SDL3-shared
)