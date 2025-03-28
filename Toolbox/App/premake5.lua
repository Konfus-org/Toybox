project "App"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/App/PCH.h"
    pchsource "Source/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.ModernJSON}"
    }

    links
    {
        "Core",
        "ModernJSON",

        -- Set links for modules so we don't have to manually rebuild when they change
        --"OpenGL Rendering",
        --"GLFW Windowing",
        --"GLFW Input",
        --"Spd Logging"
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"

    ToolboxProjectConfigs()