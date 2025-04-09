project "Simple Test App Host"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../../../" .. OutputTargetDir .. "")
    objdir    ("../../../../" .. OutputIntermediateDir .. "")

    defines
    {
        -- Simple define here, but can be used to define diff paths for diff configs, i.e. release vs debug
        "PATH_TO_PLUGINS=\"..\\\\..\\\\..\\\\..\\\\Build\\\\bin\\\\Plugins\""
    }

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    links
    {
        "Simple Test App"
    }

    includedirs
    {
        "%{IncludeDir.TestApp}",
    }

    UsingToolboxConfigs()
