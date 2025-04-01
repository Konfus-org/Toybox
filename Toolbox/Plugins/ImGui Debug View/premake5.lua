project "ImGui Debug View"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../../" .. OutputTargetPluginDir .. "")
    objdir    ("../../../" .. OutputIntermediatePluginDir .. "")

    files
    {
        "./**.h",
        "./**.c",
        "./**.hpp",
        "./**.cpp"
    }

    defines
    {
        "GLFW_INCLUDE_NONE"
    }

    includedirs
    {
        "./",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.GLFW}"
    }

    links
    {
        "GLFW",
        "ImGui"
    }

    ToolboxPluginConfigs()
