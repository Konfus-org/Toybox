project "SpdLog Module"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    warnings "Default"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../../" .. OutputDir .. "/bin/Modules/")
    objdir    ("../../../" .. OutputDir .. "/obj/Modules/")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp",
        "./**.cs",
    }

    includedirs
    {
        "./",
        "%{IncludeDir.spdlog}"
    }

    links
    {
        "spdlog"
    }

    -- Setup standard platforms and configs
    ToyboxModuleConfigs()
