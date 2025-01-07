project "Runtime"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    targetdir ("../../" .. OutputTargetDir .. "")
    objdir    ("../../" .. OutputIntermediateDir .. "")

    defines
    {
        "TOYBOX"
    }

    includedirs
    {
        "./"
    }

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    -- Set links for modules so we don't have to manually rebuild when they change
    links
    {
        "OpenGL Rendering Module",
        "GLFW Window Module",
        "GLFW Input Module",
        "SpdLog Module"
    }

    ToyboxModuleConfigs()