set(module_name Math)

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
        Tbx::Common
    PRIVATE
        glm
)

target_include_directories(${module_name}
    PUBLIC
        $<TARGET_PROPERTY:glm,INTERFACE_INCLUDE_DIRECTORIES>
)

tbx_setup_module(${module_name})
