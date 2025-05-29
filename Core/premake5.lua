project "Core"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/Core/PCH.h"
    pchsource "Source/Tbx/Core/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

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
        "./**.plugin",
        "./**.md",
        "./*.lua",
        "./*.txt"
    }
    includedirs
    {
        "./Include",
        "./Source",
        _MAIN_SCRIPT_DIR .. "/Dependencies/stbimg/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/nlohmann_json/include",
        table.unpack(Using.TbxCorePluginDirs)
    }
    links
    {
        "stbimg",
        "nlohmann_json",
        table.unpack(Using.TbxCorePluginLinks)
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"
    filter {}