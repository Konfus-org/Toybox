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
        "GLFW_INCLUDE_NONE",
        "IMGUI_IMPL_OPENGL_ES3"
    }

    includedirs
    {
        "./Source",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.ImGuiBackends}",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.sys_info}",
    }

    links
    {
        "GLFW",
        "ImGui",
        "sys_info"
    }

    ToolboxPluginConfigs()
