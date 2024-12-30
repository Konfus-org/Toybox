project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../" .. OutputTargetDir .. "")
    objdir    ("../" .. OutputIntermediateDir .. "")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    links
    {
        -- For testing! Remove once we have a proper rendering abstraction!
        "opengl32.lib"
    }

    ToyboxAppConfigs()