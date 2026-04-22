include_guard(GLOBAL)

function(tbx_define_default_plugins)
    if (TARGET TbxDefaultPlugins)
        return()
    endif ()

    add_library(TbxDefaultPlugins INTERFACE)
    add_library(Tbx::DefaultPlugins ALIAS TbxDefaultPlugins)

    if (NOT TARGET Tbx::Core)
        message(FATAL_ERROR "Tbx::DefaultPlugins requires Tbx::Core (call after add_subdirectory(core))")
    endif ()
    if (NOT TARGET Tbx::Plugins::SdlInputPlugin)
        message(WARNING "Tbx::DefaultPlugins could not find Tbx::Plugins::SdlInputPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::JoltPhysicsPlugin)
        message(WARNING "Tbx::DefaultPlugins could not find Tbx::Plugins::JoltPhysicsPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::SdlWindowingPlugin)
        message(WARNING "Tbx::DefaultPlugins could not find Tbx::Plugins::SdlWindowingPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::OpenGlRenderingPlugin)
        message(WARNING "Tbx::DefaultPlugins could not find Tbx::Plugins::OpenGlRenderingPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::StbImageLoaderPlugin)
        message(WARNING "Tbx::DefaultPlugins could not find Tbx::Plugins::StbImageLoaderPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::AssimpModelLoaderPlugin)
        message(WARNING "Tbx::DefaultPlugins could not find Tbx::Plugins::AssimpModelLoaderPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::GlslShaderLoaderPlugin)
        message(WARNING "Tbx::DefaultPlugins could not find Tbx::Plugins::GlslShaderLoaderPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::MatMaterialLoaderPlugin)
        message(WARNING "Tbx::DefaultPlugins could not find Tbx::Plugins::MatMaterialLoaderPlugin (call after add_subdirectory(plugins))")
    endif ()
    if (NOT TARGET Tbx::Plugins::SdlOpenGlContextManagerPlugin)
        message(WARNING "Tbx::DefaultPlugins could not find Tbx::Plugins::SdlOpenGlContextManagerPlugin (call after add_subdirectory(plugins))")
    endif ()

    target_link_libraries(TbxDefaultPlugins INTERFACE
            Tbx::Core
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
