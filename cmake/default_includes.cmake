include_guard(GLOBAL)

function(tbx_define_default_includes)
    if (TARGET TbxDefaultIncludes)
        return()
    endif ()

    add_library(TbxDefaultIncludes INTERFACE)
    add_library(Tbx::DefaultIncludes ALIAS TbxDefaultIncludes)

    if (NOT TARGET Tbx::App)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::App (call after add_subdirectory(modules))")
    endif ()
    if (NOT TARGET Tbx::Assets)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::Assets (call after add_subdirectory(modules))")
    endif ()
    if (NOT TARGET Tbx::Input)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::Input (call after add_subdirectory(modules))")
    endif ()
    if (NOT TARGET Tbx::ECS)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::ECS (call after add_subdirectory(modules))")
    endif ()
    if (NOT TARGET Tbx::Graphics)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::Graphics (call after add_subdirectory(modules))")
    endif ()
    if (NOT TARGET Tbx::Physics)
        message(FATAL_ERROR "Tbx::DefaultIncludes requires Tbx::Physics (call after add_subdirectory(modules))")
    endif ()
    if (NOT TARGET Tbx::Plugins::SdlInputPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::SdlInputPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::JoltPhysicsPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::JoltPhysicsPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::SdlWindowingPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::SdlWindowingPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::OpenGlRenderingPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::OpenGlRenderingPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::StbImageLoaderPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::StbImageLoaderPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::AssimpModelLoaderPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::AssimpModelLoaderPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::GlslShaderLoaderPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::GlslShaderLoaderPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::MatMaterialLoaderPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::MatMaterialLoaderPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::SdlOpenGlContextManagerPlugin)
        message(WARNING "Tbx::DefaultIncludes could not find Tbx::Plugins::SdlOpenGlContextManagerPlugin (call after add_subdirectory(plugins))")
    endif ()

    target_link_libraries(TbxDefaultIncludes INTERFACE
            Tbx::App
            Tbx::Assets
            Tbx::Input
            Tbx::ECS
            Tbx::Graphics
            Tbx::Physics
            Tbx::Plugins::SdlInputPlugin
            Tbx::Plugins::JoltPhysicsPlugin
            Tbx::Plugins::SdlWindowingPlugin
            Tbx::Plugins::SdlOpenGlContextManagerPlugin
            Tbx::Plugins::OpenGlRenderingPlugin
            Tbx::Plugins::StbImageLoaderPlugin
            Tbx::Plugins::AssimpModelLoaderPlugin
            Tbx::Plugins::GlslShaderLoaderPlugin
            Tbx::Plugins::MatMaterialLoaderPlugin
    )
endfunction()
