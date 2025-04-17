IncludeDir = {}

-- 3rd party
IncludeDir["glm"] = "%{wks.location}/3rd Party/Libraries/glm"
IncludeDir["spdlog"] = "%{wks.location}/3rd Party/Libraries/spdlog/include"
IncludeDir["GLFW"] = "%{wks.location}/3rd Party/Libraries/GLFW/include"
IncludeDir["glad"] = "%{wks.location}/3rd Party/Libraries/glad/include"
IncludeDir["ModernJSON"] = "%{wks.location}/3rd Party/Libraries/ModernJSON/single_include"
IncludeDir["ImGui"] = "%{wks.location}/3rd Party/Libraries/ImGui"
IncludeDir["ImGuiBackends"] = "%{wks.location}/3rd Party/Libraries/ImGui/backends"
IncludeDir["stbimg"] = "%{wks.location}/3rd Party/Libraries/stbimg/include"
IncludeDir["sys_info"] = "%{wks.location}/3rd Party/Libraries/sys_info/core/include/"

-- Toybox
IncludeDir["TbxCore"] = "%{wks.location}/Core/Include"
IncludeDir["TbxRuntime"] = "%{wks.location}/Runtime/Include"
IncludeDir["TbxLoader"] = "%{wks.location}/Loader/Include"

-- Testing
IncludeDir["TestApp"] = "%{wks.location}/Tests/Simple App Test/App"