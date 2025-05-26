project "Core"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    externalwarnings "Off"

    targetdir ("../" .. OutputTargetDir .. "")
    objdir    ("../" .. OutputIntermediateDir .. "")

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
        "stbimg",
        "ModernJSON",
        "googletest",
        "googlemock",
        table.unpack(Using.TbxCorePluginLinks)
    }

    includedirs
    {
        "%{Using.ModernJSON}",
        "%{Using.stbimg}",
        "%{Using.googletest}",
        "%{Using.googletest}/include",
        "%{Using.googlemock}",
        "%{Using.googlemock}/include",
        table.unpack(Using.TbxCorePluginDirs)
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"

    ToyboxProjectConfigs()