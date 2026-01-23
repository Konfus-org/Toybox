tbx_add_module(Math)

tbx_register_module(Math)

target_link_libraries(Math
    PUBLIC
        glm
        Tbx::Common
)
