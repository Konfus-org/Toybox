project "Graphics"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/Graphics/PCH.h"
    pchsource "Source/Tbx/Graphics/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

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
        _MAIN_SCRIPT_DIR .. "/Dependencies/stbimg/include",
        table.unpack(Using.TbxCorePluginDirs)
    }
    links
    {
        "stbimg",
        table.unpack(Using.TbxCorePluginLinks)
    }