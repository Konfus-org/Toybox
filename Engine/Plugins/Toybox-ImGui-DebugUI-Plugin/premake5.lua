project "ImGui Debug UI"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    RegisterDynamicPlugin("ImGui Debug UI")

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
        _MAIN_SCRIPT_DIR .. "/Dependencies/ImGui",
        _MAIN_SCRIPT_DIR .. "/Dependencies/ImGui/backends",
        _MAIN_SCRIPT_DIR .. "/Dependencies/sys_info/core/include"
    }
    links
    {
        "ImGui",
        "sys_info"
    }
    defines
    {
        "IMGUI_IMPL_OPENGL_ES3"
    }
