project "Runtime"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxApp}",
        "%{IncludeDir.ModernJSON}"
    }

    links
    {
        "Core",
        "App",
        "ModernJSON",

        -- Set links for modules so we don't have to manually rebuild when they change
        "GLFW Input",
        "GLFW Windowing",
        "ImGui Debug View",
        "OpenGL Renderer",
        "Spd Logging"
    }

    ToolboxProjectConfigs()