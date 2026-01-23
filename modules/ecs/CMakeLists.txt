tbx_register_module(ECS)

target_link_libraries(ECS
    PUBLIC
        Tbx::Common
        Tbx::Messaging
        EnTT::EnTT
)
