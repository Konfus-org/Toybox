include_guard(GLOBAL)

# tbx_define_default_includes
# --------------------------
# Defines the `Tbx::DefaultIncludes` interface target which bundles the common
# Toybox module dependencies most targets need to compile against the engine.
#
# Purpose: Provide a single, shared target for common engine includes/libs.
# Ownership: Not applicable (CMake target definition).
# Thread Safety: Not applicable.
function(tbx_define_default_includes)
    if(TARGET TbxDefaultIncludes)
        return()
    endif()

    add_library(TbxDefaultIncludes INTERFACE)
    add_library(Tbx::DefaultIncludes ALIAS TbxDefaultIncludes)

    if(NOT TARGET Tbx::App)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::App (call after add_subdirectory(modules))")
    endif()
    if(NOT TARGET Tbx::Assets)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::Assets (call after add_subdirectory(modules))")
    endif()
    if(NOT TARGET Tbx::ECS)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::ECS (call after add_subdirectory(modules))")
    endif()
    if(NOT TARGET Tbx::Graphics)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::Graphics (call after add_subdirectory(modules))")
    endif()

    target_link_libraries(TbxDefaultIncludes INTERFACE
        Tbx::App
        Tbx::Assets
        Tbx::ECS
        Tbx::Graphics
    )
endfunction()

