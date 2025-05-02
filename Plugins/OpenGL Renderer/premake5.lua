project "OpenGL Renderer"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }
    externalwarnings "Off"

    targetdir ("../../" .. OutputTargetPluginDir .. "")
    objdir    ("../../" .. OutputIntermediatePluginDir .. "")

    defines
    {
        "GLFW_INCLUDE_NONE"
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
        "./Source",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.glad}"
    }

    links
    {
        "GLFW",
        "glad"
    }

    ToyboxPluginConfigs()