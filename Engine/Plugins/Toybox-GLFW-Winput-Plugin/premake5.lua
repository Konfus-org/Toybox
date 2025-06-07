project "GLFW Winput"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    RegisterDynamicPlugin("GLFW Winput")

    files
    {
        "./**.hpp",
        "./**.cpp",
        "./**.h",
        "./**.c",
        "./**.md",
        "./**.plugin"
    }
    includedirs
    {
        "./Source",
        _MAIN_SCRIPT_DIR .. "/Dependencies/GLFW/include"
    }
    links
    {
        "GLFW"
    }

    -- Expose native platform methods 
    -- which is needed to grab handles of windows for editor
    filter "system:Windows"
        defines { "GLFW_EXPOSE_NATIVE_WIN32" }
    filter "system:Linux"
        defines { "GLFW_EXPOSE_NATIVE_WAYLAND" }
    filter "system:Macosx"
        defines { "GLFW_EXPOSE_NATIVE_COCOA" }