project "Core"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    targetdir ("../../../" .. OutputTargetDir .. "")
    objdir    ("../../../" .. OutputIntermediateDir .. "")

    pchheader "PCH.h"
    pchsource "PCH.cpp"

    defines
    {
        "TOOLBOX",
        "GLM_ENABLE_EXPERIMENTAL"
    }

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    includedirs
    {
        "./",
        "%{IncludeDir.glm}",
        "%{IncludeDir.ModernJSON}",
        "%{IncludeDir.stbimg}"
    }

    links
    {
        "glm",
        "stbimg",
        "ModernJSON"
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"

    -- Setup standard platforms and configs
    StandardConfigs()
    PlatformConfigs()
    DllConfigs()