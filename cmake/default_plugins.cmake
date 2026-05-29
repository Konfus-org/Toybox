include_guard(GLOBAL)

function(tbx_define_default_plugins)
    if(TARGET TbxDefaultPlugins)
        return()
    endif()

    add_library(TbxDefaultPlugins INTERFACE)
    add_library(Tbx::DefaultPlugins ALIAS TbxDefaultPlugins)

    if(NOT TARGET Tbx::Engine)
        message(FATAL_ERROR "Tbx::DefaultPlugins requires Tbx::Engine (call after add_subdirectory(engine))")
    endif()

    set(default_includes
        Tbx::Plugins::TbxPerformanceMonitorPlugin
        Tbx::Plugins::SdlInputPlugin
        Tbx::Plugins::JoltPhysicsPlugin
        Tbx::Plugins::SdlWindowingPlugin
        Tbx::Plugins::SdlOpenGlContextManagerPlugin
        Tbx::Plugins::OpenGlRenderingPlugin
        Tbx::Plugins::StbImageLoaderPlugin
        Tbx::Plugins::AssimpModelLoaderPlugin
        Tbx::Plugins::ShaderIncludeLoader
    )

    foreach(default_include IN LISTS default_includes)
        if(NOT TARGET ${default_include})
            message(WARNING
                "Tbx::DefaultPlugins could not find ${default_include} (call after add_subdirectory(plugins))")
        endif()
    endforeach()

    target_link_libraries(TbxDefaultPlugins INTERFACE
        Tbx::Engine
        ${default_includes}
    )
endfunction()
