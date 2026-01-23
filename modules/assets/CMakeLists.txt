tbx_register_module(Assets)

target_link_libraries(Assets
    PUBLIC
        Tbx::Common
        Tbx::Messaging
)
