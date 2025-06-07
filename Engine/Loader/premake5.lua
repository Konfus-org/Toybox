project "Loader"
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
        "../Application/Include",
        "../Debug/Include",
        "../Events/Include",
        "../Math/Include",
        "../Pipelines/Include",
        "../Servers/Include",
        "../TBS/Include",
        table.unpack(Using.TbxCorePluginDirs)
    }
    links
    {
        "Core",
        "Runtime",
        table.unpack(Using.TbxPluginLinks),
        table.unpack(Using.TbxCorePluginLinks)
    }