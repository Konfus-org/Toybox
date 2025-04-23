project "Simple Test App Host"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../../" .. OutputTargetDir .. "")
    objdir    ("../../../" .. OutputIntermediateDir .. "")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    defines
    {
        "PATH_TO_PLUGINS=\"../../Build/bin/Plugins/\""
    }

    links
    {
        "Simple Test App"
    }

    includedirs
    {
        "%{IncludeDir.TestApp}",
    }

    UsingToyboxConfigs()
