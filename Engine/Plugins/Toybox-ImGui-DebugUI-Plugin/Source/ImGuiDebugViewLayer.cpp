#include "ImGuiDebugViewLayer.h"
#include <Tbx/Application/App/App.h>
#include <Tbx/Systems/Input/Input.h>
#include <Tbx/Systems/Rendering/Rendering.h>
#include <Tbx/Systems/Windowing/WindowManager.h>
#include <Tbx/Systems/Events/EventCoordinator.h>
#include <Tbx/Systems/TBS/World.h>
#include <Tbx/Graphics/Camera.h>
#include <Tbx/Math/Transform.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>
#include <nvrhi/nvrhi.h>
#ifdef TBX_PLATFORM_WINDOWS
#include <imgui_impl_dx12.h>
#include <nvrhi/d3d12.h>
#endif
#include <nvrhi/vulkan.h>

namespace ImGuiDebugView
{
    void ImGuiDebugViewLayer::OnAttach()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // optional

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();
        
        // Init size
        const auto mainWindow = Tbx::WindowManager::GetMainWindow();
        _windowResolution = mainWindow->GetSize();

        // Setup Platform/Renderer backends
        auto* nativeWindow = static_cast<GLFWwindow*>(mainWindow->GetNativeWindow());
        _graphicsDevice = static_cast<nvrhi::IDevice*>(Tbx::Rendering::GetRenderer(mainWindow->Id)->GetGraphicsDevice());

        if (Tbx::App::GetInstance()->GetGraphicsSettings().Api == Tbx::GraphicsApi::DirectX12)
        {
            //// DX12 ////

#ifdef TBX_PLATFORM_WINDOWS

            // Get device from NVRHI
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = 1;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            desc.NodeMask = 0;

            ID3D12DescriptorHeap* imguiDescHeap = nullptr; 
            ID3D12Device* dxDevice = _graphicsDevice->getNativeObject(nvrhi::ObjectTypes::D3D12_Device);
            dxDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&imguiDescHeap));

            // Init ImGui
            ImGui_ImplGlfw_InitForOther(nativeWindow, true);
            ImGui_ImplDX12_Init(
                dxDevice,
                2,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                imguiDescHeap,
                imguiDescHeap->GetCPUDescriptorHandleForHeapStart(),
                imguiDescHeap->GetGPUDescriptorHandleForHeapStart());
#endif
        }
        else
        {
            //// Vulkan ////

            // Get required vulkan info from NVRHI
            VkDevice vkDevice = _graphicsDevice->getNativeObject(nvrhi::ObjectTypes::VK_Device);
            VkPhysicalDevice vkPhysicalDevice = _graphicsDevice->getNativeObject(nvrhi::ObjectTypes::VK_PhysicalDevice);
            VkInstance vkInstance = _graphicsDevice->getNativeObject(nvrhi::ObjectTypes::VK_Instance);
            VkQueue vkQueue = _graphicsDevice->getNativeQueue(nvrhi::ObjectTypes::VK_Queue, nvrhi::CommandQueue::Graphics);

            // Calculate graphics queue family index
            uint32_t vkQueueFamily = 0;
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(vkInstance, &deviceCount, nullptr);
            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(vkInstance, &deviceCount, devices.data());
            for (const auto& dev : devices)
            {
                if (dev != vkPhysicalDevice) continue;

                uint32_t queueCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueCount, nullptr);
                std::vector<VkQueueFamilyProperties> queueProps(queueCount);
                vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueCount, queueProps.data());

                for (uint32_t i = 0; i < queueCount; ++i)
                {
                    if (!(queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;

                    vkQueueFamily = i;
                    break;
                }
            }

            // Init ImGui vulkan backend
            ImGui_ImplVulkan_InitInfo imGuiVKInitInfo = {};
            imGuiVKInitInfo.Instance = vkInstance;
            imGuiVKInitInfo.PhysicalDevice = vkPhysicalDevice;
            imGuiVKInitInfo.Device = vkDevice;
            imGuiVKInitInfo.QueueFamily = vkQueueFamily;
            imGuiVKInitInfo.Queue = vkQueue;
            imGuiVKInitInfo.DescriptorPoolSize = 2;
            imGuiVKInitInfo.MinImageCount = 3; // We use triple buffer
            imGuiVKInitInfo.ImageCount = 3;
            imGuiVKInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
            imGuiVKInitInfo.Allocator = nullptr;
            imGuiVKInitInfo.CheckVkResultFn = nullptr;
            imGuiVKInitInfo.UseDynamicRendering = true;
            imGuiVKInitInfo.PipelineRenderingCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
            ImGui_ImplGlfw_InitForVulkan(nativeWindow, true);
            ImGui_ImplVulkan_Init(&imGuiVKInitInfo);

            // Create fonts
            ImGui_ImplVulkan_CreateFontsTexture();
        }

        // Sub to frame rendered event so we know when to draw
        _frameRenderedEventId = Tbx::EventCoordinator::Subscribe<Tbx::RenderedFrameEvent>(TBX_BIND_FN(OnFrameRendered));
        _windowResizedEventId = Tbx::EventCoordinator::Subscribe<Tbx::WindowResizedEvent>(TBX_BIND_FN(OnWindowResized));
    }

    void ImGuiDebugViewLayer::OnDetach()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::RenderedFrameEvent>(_frameRenderedEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowResizedEvent>(_windowResizedEventId);

        if (Tbx::App::GetInstance()->GetGraphicsSettings().Api == Tbx::GraphicsApi::DirectX12)
        {
            // DX12
#ifdef TBX_PLATFORM_WINDOWS
            ImGui_ImplDX12_Shutdown();
#endif
        }
        else
        {
            // Vulkan
            ImGui_ImplVulkan_Shutdown();
        }

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiDebugViewLayer::OnUpdate()
    {
        auto memory = _processInfo.GetMemoryUsage();
        auto maxMemory = _systemInfo.GetTotalMemory();
        auto memUsage = _systemInfo.GetTotalUsageMemory();
        auto cpuUsage = _processInfo.GetCpuUsage();
        auto cpuSysUsage = _systemInfo.GetCpuTotalUsage();

        // Get io from last frame
        const ImGuiIO& io = ImGui::GetIO();

        // Start the Dear ImGui frame
        {
            if (Tbx::App::GetInstance()->GetGraphicsSettings().Api == Tbx::GraphicsApi::DirectX12)
            {
                // DX12
#ifdef TBX_PLATFORM_WINDOWS
                ImGui_ImplDX12_NewFrame();
#endif
            }
            else
            {
                // Vulkan
                ImGui_ImplVulkan_NewFrame();
            }
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
        }

        // Listen for key press to toggle the debug window
        {
            if (Tbx::Input::IsKeyDown(TBX_KEY_F1))
            {
                _showDebugWindowOnDebugBtnUp = true;
            }
            if (_showDebugWindowOnDebugBtnUp &&
                Tbx::Input::IsKeyUp(TBX_KEY_F1))
            {
                _showDebugWindowOnDebugBtnUp = false;
                _isDebugWindowOpen = !_isDebugWindowOpen;
            }
        }

        // Show debug window
        if (_isDebugWindowOpen)
        {
            ImGui::Begin("Tbx Debugger", &_isDebugWindowOpen);

            if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
                ImGui::Text("CPU Usage: %.1f%%", cpuUsage);
                ImGui::Text("System CPU Usage: %.1f%%", cpuSysUsage);
                ImGui::Text("Memory Usage (Available): %.1f%%", static_cast<float>(memory) / maxMemory * 100.0f);
                ImGui::Text("Memory Usage (In MB): %.1f MB", static_cast<float>(memory) / 1024.0f / 1024.0f);
                ImGui::Text("System Memory Usage: %.1f MB", static_cast<float>(memUsage) / 1024.0f / 1024.0f);
            }

            if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Resolution: (%d, %d)", _windowResolution.Width, _windowResolution.Height);
                ImGui::Text("Aspect Ratio: (%.1f)", _windowResolution.GetAspectRatio());
            }

            if (ImGui::CollapsingHeader("Rendering"), ImGuiTreeNodeFlags_DefaultOpen)
            {
                int cameraNumber = 0;
                auto playSpaces = Tbx::World::GetPlaySpaces();
                for (auto playSpace : playSpaces)
                {
                    for (auto camera : Tbx::PlayspaceView<Tbx::Camera>(playSpace))
                    {
                        auto& camTrans = playSpace->GetBlockOn<Tbx::Transform>(camera);
                        ImGui::Text("Camera %d Position: %s", cameraNumber, camTrans.Position.ToString().c_str());
                        ImGui::Text("Camera %d Rotation: %s", cameraNumber, Tbx::Quaternion::ToEuler(camTrans.Rotation).ToString().c_str());
                        ImGui::Text("Camera %d Scale: %s", cameraNumber, camTrans.Scale.ToString().c_str());
                        cameraNumber++;
                    }
                }
            }

            if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Mouse Position: (%.1f, %.1f)", io.MousePos.x, io.MousePos.y);
                ImGui::Text("Mouse Delta: (%.1f, %.1f)", io.MouseDelta.x, io.MouseDelta.y);
            }

            ImGui::End();
        }

        // Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        //ImGui::ShowDemoWindow(&_showDemoWindow);

        // Finally, render
        ImGui::Render();
    }

    bool ImGuiDebugViewLayer::IsOverlay()
    {
        return true;
    }

    void ImGuiDebugViewLayer::OnFrameRendered(const Tbx::RenderedFrameEvent&) const
    {
        if (ImGui::GetDrawData() == nullptr) return;

        // This needs to be called after the frame has been rendered by the TBX renderer
        if (Tbx::App::GetInstance()->GetGraphicsSettings().Api == Tbx::GraphicsApi::DirectX12)
        {
            // DX12
#ifdef TBX_PLATFORM_WINDOWS
            auto* dxDevice = dynamic_cast<nvrhi::d3d12::IDevice*>(_graphicsDevice);
            ID3D12GraphicsCommandList* dxCommandList = dxDevice->getNativeObject(nvrhi::ObjectTypes::D3D12_GraphicsCommandList);
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommandList);
#endif
        }
        else
        {
            // Vulkan
            auto* vulkDevice = dynamic_cast<nvrhi::vulkan::IDevice*>(_graphicsDevice);
            VkCommandBuffer vkCommandBuffer = vulkDevice->getNativeObject(nvrhi::ObjectTypes::VK_CommandBuffer);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCommandBuffer);
        }
    }

    void ImGuiDebugViewLayer::OnWindowResized(const Tbx::WindowResizedEvent& e)
    {
        _windowResolution = e.GetNewSize();
    }
}
