tbx_add_module(Files)

tbx_register_module(Files)

target_link_libraries(Files
    PUBLIC
        Tbx::Common
    PRIVATE
        nlohmann_json
)
