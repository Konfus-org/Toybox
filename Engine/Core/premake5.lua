project "Core"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    targetdir ("../../" .. OutputDir .. "/bin/")
    objdir    ("../../" .. OutputDir .. "/obj/")

    pchheader "tbxpch.h"
    pchsource "tbxpch.cpp"

    defines
    {
        "TOYBOX"
    }

    includedirs
    {
        "./"
    }

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"

    -- Setup standard platforms and configs
    ToyboxConfigs()
    ToyboxPlatforms()
    DllConfigs()