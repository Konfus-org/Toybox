project "Simple Test App"
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

    ToolboxPluginConfigs()