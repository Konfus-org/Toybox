project "Simple Test App"
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
        "./**.hpp",
        "./**.cpp"
    }

    includedirs
    {
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxRuntime}"
    }

    links
    {
        "Core",
        "Runtime"
    }

    defines
    {
        "COMPILING_TOYBOX"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()