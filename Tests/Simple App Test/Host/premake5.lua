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

    links
    {
        "Simple Test App",
    }

    includedirs
    {
        "%{IncludeDir.TestApp}",
    }

    UsingToolboxConfigs()