project "Launcher"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.md"
    }
    includedirs
    {
        "./Include",
        "./Source",
        "../Utils/Include",
        "../Math/Include",
        "../Graphics/Include",
        "../Application/Include",
        "../Systems/Include",
        table.unpack(Using.TbxCorePluginDirs)
    }
    links
    {
        "Utils",
        "Math",
        "Graphics",
        "Application",
        "Systems",
        table.unpack(Using.TbxCorePluginLinks),
        table.unpack(Using.TbxPluginLinks)
    }
    defines
    {
        "PATH_TO_PLUGINS=\"" .. OutputTargetDir .. "\""
    }