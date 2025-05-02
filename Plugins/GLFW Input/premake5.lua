project "GLFW Input"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }
    externalwarnings "Off"

    targetdir ("../../" .. OutputTargetPluginDir .. "")
    objdir    ("../../" .. OutputIntermediatePluginDir .. "")

    defines
    {
        "GLFW_INCLUDE_NONE"
    }

    includedirs
    {
        "./Source",
        "%{IncludeDir.GLFW}"
    }

    links
    {
        "GLFW"
    }

    ToyboxPluginConfigs()

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