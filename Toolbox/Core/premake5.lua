project "Core"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/Core/PCH.h"
    pchsource "Source/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

    links
    {
        "glm",
        "stbimg",
        "ModernJSON"
    }

    includedirs
    {
        "%{IncludeDir.glm}",
        "%{IncludeDir.ModernJSON}",
        "%{IncludeDir.stbimg}"
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"

    ToolboxProjectConfigs()