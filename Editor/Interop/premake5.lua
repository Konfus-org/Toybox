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

    ToyboxAppConfigs()

    -- Editor Supported Platforms
    filter "system:Windows"
        defines "TOYBOX_EDITOR"
