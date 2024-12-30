project "OpenGL Rendering Module"
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

    defines
    {
        "GLFW_INCLUDE_NONE"
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