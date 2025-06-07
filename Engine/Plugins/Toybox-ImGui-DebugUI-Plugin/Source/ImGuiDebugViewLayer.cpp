#include "ImGuiDebugViewLayer.h"
#include <Tbx/Application/App/App.h>
#include <Tbx/Application/Input/Input.h>
#include <Tbx/Application/Windowing/WindowManager.h>
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Core/Rendering/Camera.h>
#include <Tbx/Core/TBS/World.h>
#include <Tbx/Math/Transform.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>
#ifdef TBX_PLATFORM_WINDOWS
#include <imgui_impl_dx12.h>
#endif

namespace ImGuiDebugView
{
    void ImGuiDebugViewLayer::OnAttach()
    {
        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();

        // Init size
        const auto mainWindow = Tbx::WindowManager::GetMainWindow();
        _windowResolution = mainWindow.lock()->GetSize();

        // Setup Platform/Renderer backends
        auto* nativeWindow = std::any_cast<GLFWwindow*>(mainWindow.lock()->GetNativeWindow());

        if (Tbx::App::Instance->GetGraphicsSettings().Api == Tbx::GraphicsApi::DirectX)
        {
#ifdef TBX_PLATFORM_WINDOWS
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc.NumDescriptors = 1;
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            desc.NodeMask = 0;

            ID3D12DescriptorHeap* imguiDescHeap = nullptr;
            device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&imguiDescHeap));

            // DirectX
            ImGui_ImplGlfw_InitForOther(nativeWindow, true); 
            ImGui_ImplDX12_Init(
                device,
                numFramesInFlight,                     // usually your backbuffer count
                DXGI_FORMAT_R8G8B8A8_UNORM,            // render target format
                imguiDescHeap,                         // the descriptor heap
                imguiDescHeap->GetCPUDescriptorHandleForHeapStart(),
                imguiDescHeap->GetGPUDescriptorHandleForHeapStart()
            );
#endif
        }
        else
        {
            // Vulkan
            ImGui_ImplGlfw_InitForVulkan(nativeWindow, true);
            ImGui_ImplVulkan_Init();
        }

        // Sub to frame rendered event so we know when to draw
        _frameRenderedEventId = Tbx::EventCoordinator::Subscribe<Tbx::RenderedFrameEvent>(TBX_BIND_FN(OnFrameRendered));
        _windowResizedEventId = Tbx::EventCoordinator::Subscribe<Tbx::WindowResizedEvent>(TBX_BIND_FN(OnWindowResized));
    }

    void ImGuiDebugViewLayer::OnDetach()
    {
        Tbx::EventCoordinator::Unsubscribe<Tbx::RenderedFrameEvent>(_frameRenderedEventId);
        Tbx::EventCoordinator::Unsubscribe<Tbx::WindowResizedEvent>(_windowResizedEventId);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
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
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Listen for key press to toggle the debug window
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
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void ImGuiDebugViewLayer::OnWindowResized(const Tbx::WindowResizedEvent& e)
    {
        _windowResolution = e.GetNewSize();
    }
}
