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
    if(NOT TARGET Tbx::Plugins::SdlInputPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::SdlInputPlugin (call after add_subdirectory(plugins))")
    endif()
    if(NOT TARGET Tbx::Plugins::SdlWindowingPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::SdlWindowingPlugin (call after add_subdirectory(plugins))")
    endif()
    if(NOT TARGET Tbx::Plugins::OpenGlRenderingPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::OpenGlRenderingPlugin (call after add_subdirectory(plugins))")
    endif()
    if(NOT TARGET Tbx::Plugins::StbImageLoaderPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::StbImageLoaderPlugin (call after add_subdirectory(plugins))")
    endif()
    if(NOT TARGET Tbx::Plugins::AssimpModelLoaderPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::AssimpModelLoaderPlugin (call after add_subdirectory(plugins))")
    endif()
    if(NOT TARGET Tbx::Plugins::GlslShaderLoaderPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::GlslShaderLoaderPlugin (call after add_subdirectory(plugins))")
    endif()
    if(NOT TARGET Tbx::Plugins::MatMaterialLoaderPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::MatMaterialLoaderPlugin (call after add_subdirectory(plugins))")
    endif()
    if (NOT TARGET Tbx::Plugins::SdlOpenGlAdapterPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::SdlOpenGlAdapterPlugin (call after add_subdirectory(plugins))")
    endif()

    target_link_libraries(TbxDefaultIncludes INTERFACE
        Tbx::App
        Tbx::Assets
        Tbx::ECS
        Tbx::Graphics
        Tbx::Plugins::SdlInputPlugin
        Tbx::Plugins::SdlWindowingPlugin
        Tbx::Plugins::OpenGlRenderingPlugin
        Tbx::Plugins::SdlOpenGlAdapterPlugin
        Tbx::Plugins::StbImageLoaderPlugin
        Tbx::Plugins::AssimpModelLoaderPlugin
        Tbx::Plugins::GlslShaderLoaderPlugin
        Tbx::Plugins::MatMaterialLoaderPlugin
    )
endfunction()
