project "OpenGL Renderer"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../../" .. OutputTargetPluginDir .. "")
    objdir    ("../../../" .. OutputIntermediatePluginDir .. "")

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
        "./",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.glad}"
    }

    links
    {
        "GLFW",
        "glad"
    }

    ToolboxPluginConfigs()