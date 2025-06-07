project "Pipelines"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/Pipelines/PCH.h"
    pchsource "Source/Tbx/Pipelines/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.md"
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