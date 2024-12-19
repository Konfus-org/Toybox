#pragma once
#include <Core.h>
#include <GLFW/glfw3.h>

namespace GlfwWindowing
{
    class GlfwWindow : public Toybox::IWindow
    {
    public:
        GlfwWindow();
        ~GlfwWindow() final;

        void Open(Toybox::WindowMode mode) override;
        void Update() override;

        void SetVSyncEnabled(bool enabled) override;
        bool GetVSyncEnabled() const override;

        void SetSize(Toybox::Size size) override;
        Toybox::Size GetSize() const override;

        std::string GetTitle() const override;
        void SetTitle(const std::string& title) override;

        Toybox::uint64 GetId() const override;
        std::any GetNativeWindow() const override;

        void SetEventCallback(const EventCallbackFn& callback) override;
        void SetMode(Toybox::WindowMode mode) override;

    private:
        GLFWwindow* _glfwWindow = nullptr;
        EventCallbackFn _eventCallback;
        std::string _title;
        Toybox::Size _size;
        bool _vSyncEnabled;

        void SetupCallbacks();
        void SetupContext();
    };
}
