#pragma once
#include <GLFW/glfw3.h>
#include "../IWindow.h"

namespace Toybox::Modules
{
    class TOYBOX_API GlfwWindowsWindow : public IWindow
    {
    public:
        GlfwWindowsWindow(std::string title, Math::Size* size);
        ~GlfwWindowsWindow() override;
        void Update() override;
        void SetVSyncEnabled(bool enabled) override;
        bool GetVSyncEnabled() const override;
        Math::Size* GetSize() const override;
        std::string GetTitle() const override;

    private:
        std::string _title;
        Math::Size* _size;
        bool _vSyncEnabled;
        GLFWwindow* _glfwWindow;
    };
}
