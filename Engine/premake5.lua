project "Engine"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    pchheader "Tbx/PCH.h"
    pchsource "Source/Tbx/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

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
        _MAIN_SCRIPT_DIR .. "/Dependencies/glm",
        _MAIN_SCRIPT_DIR .. "/Dependencies/nlohmann_json/include",
        table.unpack(Using.TbxStaticPluginDirs)
    }
    links
    {
        "glm",
        "nlohmann_json",
        table.unpack(Using.TbxStaticPluginLinks)
    }
    defines
    {
        "GLM_ENABLE_EXPERIMENTAL",
        "GLM_FORCE_LEFT_HANDED",
        "GLM_DEPTH_ZERO_TO_ONE"
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"
    filter {}