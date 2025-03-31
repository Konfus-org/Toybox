project "App"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    targetdir ("../../" .. OutputTargetPluginDir .. "")
    objdir    ("../../" .. OutputIntermediatePluginDir .. "")

    pchheader "Tbx/App/PCH.h"
    pchsource "Source/PCH.cpp" -- Full path MUST be specified relative to the premake5.lua (this) script.

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    includedirs
    {
        "./Include",
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.ModernJSON}"
    }

    links
    {
        "Core",
        "ModernJSON",

        -- Set links for modules so we don't have to manually rebuild when they change
        --"OpenGL Rendering",
        --"GLFW Windowing",
        --"GLFW Input",
        --"Spd Logging"
    }

    defines
    {
        "TOOLBOX",
        "GLM_ENABLE_EXPERIMENTAL"
    }

    postbuildcommands
    {
        "{ECHO} Copying plugin.meta from \"%{prj.location}plugin.meta\" to \"../../%{OutputTargetPluginDir}\"",
        "{COPYFILE} \"%{prj.location}plugin.meta\" \"../../%{OutputTargetPluginDir}\""
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()