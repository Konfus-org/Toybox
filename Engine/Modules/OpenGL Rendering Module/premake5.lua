project "OpenGL Rendering Module"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"
    ignoredefaultlibraries { "MSVCRT" }

    targetdir ("../../../" .. OutputTargetModulesDir .. "")
    objdir    ("../../../" .. OutputIntermediateModulesDir .. "")

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
        "%{IncludeDir.glad}",
    }

    links
    {
        "GLFW",
        "glad"
    }

    ToyboxModuleConfigs()