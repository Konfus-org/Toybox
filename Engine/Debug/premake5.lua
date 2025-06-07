project "Debug"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/Debug/PCH.h"
    pchsource "Source/Tbx/Debug/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

    files
    {
        "./Source/**.h",
        "./Source/**.c",
        "./Source/**.cc",
        "./Source/**.hpp",
        "./Source/**.cpp",
        "./Include/**.h",
        "./Include/**.c",
        "./Include/**.cc",
        "./Include/**.hpp",
        "./Include/**.cpp",
        "./**.md"
    }
    includedirs
    {
        "./Include",
        "./Source",
        "../Systems/Include",
        "../Utils/Include",
    }
    links
    {
        "Systems",
        "Utils"
    }