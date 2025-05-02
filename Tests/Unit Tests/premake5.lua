project "Unit Tests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../" .. OutputTargetDir .. "")
    objdir    ("../../" .. OutputIntermediateDir .. "")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp",
        "./**.md",
        "./**.lua",
    }

    pchheader "PCH.h"
    pchsource "PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

    includedirs
    {
        "./",
        "%{IncludeDir.googletest}",
        "%{IncludeDir.googletest}/include",
        "%{IncludeDir.googlemock}",
        "%{IncludeDir.googlemock}/include",
    }

    links
    {
        "googletest",
        "googlemock"
    }

    UsingToyboxConfigs()