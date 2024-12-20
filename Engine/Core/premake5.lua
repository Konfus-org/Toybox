project "Core"
    kind "StaticLib"
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

    -- Setup standard platforms and configs
    ToyboxConfigs()
    ToyboxPlatforms()