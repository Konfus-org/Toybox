project "SpdLog Module"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../../" .. OutputTargetModulesDir .. "")
    objdir    ("../../../" .. OutputIntermediateModulesDir .. "")

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
        "%{IncludeDir.spdlog}"
    }

    links
    {
        "spdlog"
    }

    -- Setup standard platforms and configs
    ToyboxModuleConfigs()
