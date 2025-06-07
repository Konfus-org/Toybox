project "Core Tests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "PCH.h"
    pchsource "PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp",
        "./**.md",
        "./**.lua",
    }
    includedirs
    {
        "./",
        "../Include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/googletest/googletest",
        _MAIN_SCRIPT_DIR .. "/Dependencies/googletest/googletest/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/googletest/googlemock",
        _MAIN_SCRIPT_DIR .. "/Dependencies/googletest/googlemock/include",
        table.unpack(Using.TbxCorePluginDirs)
    }
    links
    {
        "Core",
        "googletest",
        "googlemock",
        table.unpack(Using.TbxCorePluginLinks)
    }