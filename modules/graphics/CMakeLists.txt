tbx_add_module(Graphics)

tbx_register_module(Graphics)

target_link_libraries(Graphics
    PUBLIC
        Tbx::Debugging
        glm
)
