project "Simple App"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }
    externalwarnings "Off"

    targetdir ("../../../" .. OutputTargetPluginDir .. "")
    objdir    ("../../../" .. OutputIntermediatePluginDir .. "")

    postbuildcommands
    {
        "{ECHO} Copying plugin meta from \"%{prj.location}%{prj.name}.plugin\" to \"../../../%{OutputTargetPluginDir}\"",
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"../../../%{OutputTargetPluginDir}\""
    }

    files
    {
        "./**.h",
        "./**.c",
        "./**.cc",
        "./**.hpp",
        "./**.cpp",
        "./**.md",
        "./**.lua",
        "./**.plugin"
    }

    includedirs
    {
        "%{Using.TbxCore}",
        "%{Using.TbxRuntime}"
    }

    links
    {
        "Core",
        "Runtime",

        -- Set links for modules so we don't have to manually rebuild when they change
        "GLFW Input",
        "GLFW Windowing",
        "ImGui Debug View",
        "OGRE Renderer",
        "Spd Logging"
    }

    defines
    {
        "COMPILING_TOYBOX"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()