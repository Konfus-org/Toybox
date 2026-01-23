tbx_add_module(Debugging)

tbx_register_module(Debugging)

target_link_libraries(Debugging
    PUBLIC
        Tbx::Common
        Tbx::Messaging
        Tbx::Files
)
