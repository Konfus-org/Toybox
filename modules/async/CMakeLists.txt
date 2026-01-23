tbx_add_module(Async)

tbx_register_module(Async)

target_link_libraries(Async
    PUBLIC
        Tbx::Common
)
