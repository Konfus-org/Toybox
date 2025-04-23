project "Runtime"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/Runtime/PCH.h"
    pchsource "Source/Tbx/Runtime/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

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

    ToyboxProjectConfigs()