project "Runtime"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    targetdir ("../../" .. OutputDir .. "/bin/")
    objdir    ("../../" .. OutputDir .. "/obj/")

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

    -- Set dependson for modules so we don't have to manually rebuild when they change
    links
    {
        "Glfw Window Module",
        "Glfw Input Module",
        "SpdLog Module"
    }

    -- To debug loading shared libs at runtime
    filter "system:Windows"
        links "DbgHelp.lib"

    ToyboxModuleConfigs()