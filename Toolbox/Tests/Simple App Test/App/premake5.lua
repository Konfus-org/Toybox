project "Simple Test App"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../../../" .. OutputTargetPluginDir .. "")
    objdir    ("../../../../" .. OutputIntermediatePluginDir .. "")

    postbuildcommands
    {
        "{ECHO} Copying plugin meta from \"%{prj.location}%{prj.name}.plugin\" to \"../../../../%{OutputTargetPluginDir}\"",
        "{COPYFILE} \"%{prj.location}%{prj.name}.plugin\" \"../../../../%{OutputTargetPluginDir}\""
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
        "%{IncludeDir.TbxApp}"
    }

    links
    {
        "Core",
        "App"
    }

    defines
    {
        "COMPILING_TOOLBOX"
    }

    PlatformConfigs()
    StandardBuildConfigs()
    DllConfigs()