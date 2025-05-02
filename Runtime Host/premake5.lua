project "RuntimeHost"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    externalwarnings "Off"

    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxRuntime}",
        "%{IncludeDir.ModernJSON}"
    }

    links
    {
        "Core",
        "Runtime",
        "ModernJSON",

        -- Set links for modules so we don't have to manually rebuild when they change
        "GLFW Input",
        "GLFW Windowing",
        "ImGui Debug View",
        "OpenGL Renderer",
        "Spd Logging"
    }

    ToyboxProjectConfigs()