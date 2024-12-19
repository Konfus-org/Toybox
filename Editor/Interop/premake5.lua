project "Interop"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    warnings "Default"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../" .. OutputDir .. "/bin/")
    objdir    ("../../" .. OutputDir .. "/obj/")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp",
        "./**.cs",
    }

    links
    {
        -- For testing! Remove once we have a proper rendering abstraction!
        "opengl32.lib"
    }

    ToyboxAppConfigs()

    -- Editor Supported Platforms
    filter "system:Windows"
        defines "TOYBOX_EDITOR"
