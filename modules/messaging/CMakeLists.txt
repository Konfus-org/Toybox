tbx_register_module(Messaging)

target_link_libraries(Messaging
    PUBLIC
        Tbx::Async
        Tbx::Time
        Tbx::Math
)
