project "Core"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    externalwarnings "Off"

    pchheader "Tbx/Core/PCH.h"
    pchsource "Source/Tbx/Core/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

    defines
    {
        "TOYBOX",
        "COMPILING_TOYBOX",
        "GLM_ENABLE_EXPERIMENTAL",
        "GLM_FORCE_LEFT_HANDED",
        "GLM_DEPTH_ZERO_TO_ONE"
    }

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

    ToyboxProjectConfigs()