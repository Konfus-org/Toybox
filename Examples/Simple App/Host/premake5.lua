project "Simple App Host"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }
    externalwarnings "Off"

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
        "PATH_TO_PLUGINS=\"../../../Build/bin/Plugins/\""
    }

    links
    {
        "Simple App"
    }

    includedirs
    {
        "%{IncludeDir.ExampleApp}",
    }

    UsingToyboxConfigs()
