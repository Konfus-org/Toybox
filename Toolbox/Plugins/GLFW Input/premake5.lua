project "GLFW Input"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    defines
    {
        "GLFW_INCLUDE_NONE"
    }

    includedirs
    {
        "./",
        "%{IncludeDir.GLFW}"
    }

    links
    {
        "GLFW"
    }

    ToolboxPluginConfigs()

    -- Expose native platform methods 
    -- which is needed to grab handles of windows for editor
    filter "system:Windows"
        defines
        {
            "GLFW_EXPOSE_NATIVE_WIN32"
        }

    filter "system:Linux"
        defines
        {
            "GLFW_EXPOSE_NATIVE_WAYLAND"
        }

    filter "system:Macosx"
        defines
        {
            "GLFW_EXPOSE_NATIVE_COCOA"
        }