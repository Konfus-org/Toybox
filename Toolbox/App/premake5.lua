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
        "ModernJSON"
    }

    ToolboxProjectConfigs()