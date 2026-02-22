set(module_name Graphics)

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
        Tbx::Debugging
        Tbx::Math
        Tbx::Messaging
        glm
)

tbx_setup_module(${module_name})
