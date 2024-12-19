project "Runtime"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../" .. OutputDir .. "/bin/%{prj.name}/")
    objdir    ("../../" .. OutputDir .. "/obj/%{prj.name}/")

    defines
    {
        "TOYBOX"
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
        "./",
        "%{IncludeDir.EngineCore}"
    }

    links
    {
        "Core"
    }

    StandardPlatforms()
    StandardConfigs()
    DllConfigs()