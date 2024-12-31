#pragma once
#include <Core.h>
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    class GLFWWindow : public Toybox::IWindow
    {
    public:
        GLFWWindow();
        ~GLFWWindow() final;

        void SetRenderer(const std::shared_ptr<Toybox::IRenderer>& renderer) override;
        void Open(const Toybox::WindowMode& mode) override;
        void Update() override;

        void SetVSyncEnabled(const bool& enabled) override;
        bool GetVSyncEnabled() const override;

        void SetSize(const Toybox::Size& size) override;
        Toybox::Size GetSize() const override;

        std::string GetTitle() const override;
        void SetTitle(const std::string& title) override;

        Toybox::uint64 GetId() const override;
        std::any GetNativeWindow() const override;

        void SetEventCallback(const Toybox::EventCallbackFn& callback) override;
        void SetMode(const Toybox::WindowMode& mode) override;

    private:
        bool _vSyncEnabled;
        std::string _title;
        Toybox::Size _size;
        Toybox::EventCallbackFn _eventCallback;
        std::shared_ptr<Toybox::IRenderer> _renderer;
        GLFWwindow* _glfwWindow = nullptr;

        void SetupCallbacks();
        void OnKeyPressed(int key, int scancode, int action, int mods) const;
        void OnMouseButtonPressed(int button, int action, int mods) const;
        void OnMouseScrolled(double offsetX, double offsetY) const;
        void OnMouseMoved(double posX, double posY) const;
        void OnWindowClosed() const;
        void OnSizeChanged();
    };
}
