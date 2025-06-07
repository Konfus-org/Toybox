#pragma once
#include <Tbx/Application/App.h>
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    class GLFWWindow final : public Tbx::IWindow
    {
    public:
        GLFWWindow();
        ~GLFWWindow() override;

        void Open(const Tbx::WindowMode& mode) override;
        void Close() override;
        void Update() override;
        void Focus() override;

        void SetMode(const Tbx::WindowMode& mode) override;

        void SetSize(const Tbx::Size& size) override;
        const Tbx::Size& GetSize() const override;

        const std::string& GetTitle() const override;
        void SetTitle(const std::string& title) override;

        std::any GetNativeWindow() const override;
        Tbx::NativeHandle GetNativeHandle() const override;

        std::shared_ptr<Tbx::IRenderer> GetRenderer() override;
        void SetRenderer(std::shared_ptr<Tbx::IRenderer> renderer) override;

    private:
        std::string _title = "";
        Tbx::Size _size = {0, 0};
        GLFWwindow* _glfwWindow = nullptr;
        std::shared_ptr<Tbx::IRenderer> _renderer = nullptr;

        void SetupCallbacks();
        void OnKeyPressed(int key, int scancode, int action, int mods) const;
        void OnMouseButtonPressed(int button, int action, int mods) const;
        void OnMouseScrolled(double offsetX, double offsetY) const;
        void OnMouseMoved(double posX, double posY) const;
        void OnWindowClosed() const;
        void OnWindowFocusChanged(bool isFocused) const;
        void OnSizeChanged() const;
    };
}
