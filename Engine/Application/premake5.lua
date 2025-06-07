project "Runtime"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/Runtime/PCH.h"
    pchsource "Source/Tbx/Runtime/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.md",
        "./**.lua"
    }
    includedirs
    {
        "./Include",
        "./Source",
        "../Core/Include",
        table.unpack(Using.TbxCorePluginDirs)
    }
    links
    {
        "Core",
        table.unpack(Using.TbxCorePluginLinks)
    }