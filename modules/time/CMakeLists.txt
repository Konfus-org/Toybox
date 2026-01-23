tbx_add_module(Time)

tbx_register_module(Time)

target_link_libraries(Time
    PUBLIC
        Tbx::Common
        Tbx::Async
)
