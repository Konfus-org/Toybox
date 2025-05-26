Using = {}

-- Dependencies
Using["glm"] = "%{wks.location}/Dependencies/glm"
Using["spdlog"] = "%{wks.location}/Dependencies/spdlog/include"
Using["GLFW"] = "%{wks.location}/Dependencies/GLFW/include"
Using["ModernJSON"] = "%{wks.location}/Dependencies/ModernJSON/single_include"
Using["ImGui"] = "%{wks.location}/Dependencies/ImGui"
Using["ImGuiBackends"] = "%{wks.location}/Dependencies/ImGui/backends"
Using["stbimg"] = "%{wks.location}/Dependencies/stbimg/include"
Using["sys_info"] = "%{wks.location}/Dependencies/sys_info/core/include/"
Using["googletest"] = "%{wks.location}/Dependencies/googletest/googletest/"
Using["googlemock"] = "%{wks.location}/Dependencies/googletest/googlemock/"
Using["DiligentEngine"] = "%{wks.location}/Dependencies/DiligentEngine/"

-- Plugins list
Using["TbxCorePluginLinks"] = {}
Using["TbxCorePluginDirs"] = {}
Using["TbxPluginLinks"] = {}

-- Toybox
Using["TbxCore"] = "%{wks.location}/Core/Include"
Using["TbxRuntime"] = "%{wks.location}/Runtime/Include"
Using["TbxMain"] = "%{wks.location}/Main/Include"