set(module_name PluginApi)

if(TBX_BUILD_SHARED)
    add_library(${module_name} SHARED)
    target_compile_definitions(${module_name}
        PUBLIC
            TBX_SHARED_LIB
        PRIVATE
            TBX_EXPORTING_SYMBOLS
    )
else()
    add_library(${module_name} STATIC)
endif()

add_library(Tbx::${module_name} ALIAS ${module_name})

target_link_libraries(${module_name}
    PUBLIC
        Tbx::Messaging
        Tbx::Debugging
        Tbx::Files
    PRIVATE
        nlohmann_json
)

target_compile_definitions(${module_name}
    PUBLIC
        TBX_PLUGIN_ABI_VERSION=${TBX_PLUGIN_ABI_VERSION}
)

tbx_setup_module(${module_name})
