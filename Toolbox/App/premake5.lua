project "App"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    targetdir ("../../" .. OutputTargetDir .. "")
    objdir    ("../../" .. OutputIntermediateDir .. "")

    defines
    {
        "TOOLBOX"
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
        "%{IncludeDir.TbxCore}"
    }

    -- Set links for modules so we don't have to manually rebuild when they change
    links
    {
        "Core",
        "OpenGL Rendering",
        "GLFW Windowing",
        "GLFW Input",
        "Spd Logging"
    }

    PlatformConfigs()
    StandardConfigs()
    DllConfigs()