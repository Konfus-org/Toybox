#include "ImGuiDebugViewLayer.h"
#include <Tbx/Runtime/Input/Input.h>
#include <Tbx/Runtime/Windowing/WindowManager.h>
#include <Tbx/Core/Events/EventCoordinator.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3_loader.h>

namespace ImGuiDebugView
{
    void ImGuiDebugViewLayer::OnAttach()
    {
        // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
        // GL ES 2.0 + GLSL 100 (WebGL 1.0)
        const char* glsl_version = "#version 100";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
        // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
        const char* glsl_version = "#version 300 es";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(TBX_PLATFORM_OSX)
        // GL 3.2 + GLSL 150
        const char* glsl_version = "#version 150";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
        // GL 3.0 + GLSL 130
        const char* glsl_version = "#version 130";
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
        //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
        //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif
        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        //ImGui::StyleColorsLight();

        // Init size
        const auto& mainWindow = Tbx::WindowManager::GetMainWindow();
        _windowResolution = mainWindow.lock()->GetSize();

        // Setup Platform/Renderer backends
        auto* nativeWindow = std::any_cast<GLFWwindow*>(mainWindow.lock()->GetNativeWindow());
        ImGui_ImplGlfw_InitForOpenGL(nativeWindow, true);
        ImGui_ImplOpenGL3_Init(glsl_version);

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

            if (ImGui::CollapsingHeader("Performance"))
            {
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
                ImGui::Text("CPU Usage: %.1f%%", cpuUsage);
                ImGui::Text("System CPU Usage: %.1f%%", cpuSysUsage);
                ImGui::Text("Memory Usage (Available): %.1f%%", (float)memory / maxMemory * 100.0f);
                ImGui::Text("Memory Usage (In MB): %.1f MB", (float)memory / 1024.0f / 1024.0f);
                ImGui::Text("System Memory Usage: %.1f MB", (float)memUsage / 1024.0f / 1024.0f);
            }

            if (ImGui::CollapsingHeader("Display"))
            {
                ImGui::Text("Resolution: (%d, %d)", _windowResolution.Width, _windowResolution.Height);
                ImGui::Text("Aspect Ratio: (%.1f)", _windowResolution.GetAspectRatio());
            }

            if (ImGui::CollapsingHeader("Input"))
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
