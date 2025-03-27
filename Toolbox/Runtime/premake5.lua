project "Runtime"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    targetdir ("../../" .. OutputTargetDir .. "")
    objdir    ("../../" .. OutputIntermediateDir .. "")

    defines
    {
        "TOOLBOX",
        "GLM_ENABLE_EXPERIMENTAL"
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
        "%{IncludeDir.TbxCore}",
        "%{IncludeDir.TbxApp}"
    }

    links
    {
        "Core",
        "App"
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"

    -- Setup standard platforms and configs
    StandardConfigs()
    PlatformConfigs()
    DllConfigs()