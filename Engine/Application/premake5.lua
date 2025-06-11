project "Application"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/Application/PCH.h"
    pchsource "Source/Tbx/Application/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

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
        "../Math/Include",
        "../Graphics/Include",
        "../Utils/Include",
        "../Systems/Include",
        table.unpack(Using.TbxCorePluginDirs)
    }
    links
    {
        "Math",
        "Graphics",
        "Utils",
        "Systems",
        table.unpack(Using.TbxCorePluginLinks)
    }