set(module_name App)

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
    PRIVATE
        Tbx::Common
        Tbx::Time
        Tbx::Input
        Tbx::PluginApi
        Tbx::ECS
        Tbx::Graphics
        Tbx::Physics
        Tbx::Assets
        Tbx::Audio
        Tbx::Resources
        Tbx::Async
        Tbx::Math
        glm
        Tbx::Messaging
        Tbx::Debugging
        EnTT::EnTT
        Tbx::Files
)

tbx_setup_module(${module_name})
