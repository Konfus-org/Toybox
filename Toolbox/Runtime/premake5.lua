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

    defines
    {
        "TOOLBOX",
        "TBX_PATH_TO_PLUGINS=\"..\\\\..\\\\..\\\\Build\\\\bin\\\\Plugins\\\\ \""
    }

    links
    {
        "Core",
        "App",
        "ModernJSON",

        -- Set links for modules so we don't have to manually rebuild when they change
        "OpenGL Renderer",
        "GLFW Windowing",
        "GLFW Input",
        "Spd Logging"
    }

    ToolboxProjectConfigs()