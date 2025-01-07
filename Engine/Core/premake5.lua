project "Core"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    targetdir ("../../" .. OutputTargetDir .. "")
    objdir    ("../../" .. OutputIntermediateDir .. "")

    pchheader "TbxPCH.h"
    pchsource "TbxPCH.cpp"

    defines
    {
        "TOYBOX",
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
    }

    links
    {
        "glm"
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"

    -- Setup standard platforms and configs
    ToyboxConfigs()
    ToyboxPlatforms()
    DllConfigs()