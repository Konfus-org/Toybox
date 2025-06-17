project "ImGui Debug UI"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "Off"

    local vulkan_sdk = os.getenv("VULKAN_SDK")
    assert(vulkan_sdk, "Vulkan SDK not found! Make sure you have Vulkan installed! After install restart PC to make the SDK env path to be available.")

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

        -- ImGui includes
        _MAIN_SCRIPT_DIR .. "/Dependencies/ImGui",
        _MAIN_SCRIPT_DIR .. "/Dependencies/ImGui/backends",

        -- sys_info includes
        _MAIN_SCRIPT_DIR .. "/Dependencies/sys_info/core/include",
        
        -- NVRHI includes
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/thirdparty/DirectX-Headers/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/thirdparty/DirectX-Headers/include/directx",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/thirdparty/Vulkan-Headers/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/rtxmu/include",
        _MAIN_SCRIPT_DIR .. "/Dependencies/NVRHI/include"
    }
    libdirs
    {
        vulkan_sdk .. "/Lib" 
    }
    links
    {
        "ImGui",
        "NVRHI",
        "sys_info",
        "vulkan-1"
    }

    RegisterDynamicPlugin("ImGui Debug UI")
