#pragma once
#include <TbxCore.h>
#include <GLFW/glfw3.h>

namespace GLFWWindowing
{
    class GLFWWindow : public Tbx::IWindow
    {
    public:
        GLFWWindow();
        ~GLFWWindow() final;
        void Open(const Tbx::WindowMode& mode) override;
        void Close() override;
        void Update() override;

        std::weak_ptr<Tbx::Camera> GetCamera() const override;

        void SetSize(const Tbx::Size& size) override;
        const Tbx::Size& GetSize() const override;

        const std::string& GetTitle() const override;
        void SetTitle(const std::string& title) override;

        Tbx::ID GetId() const override;
        std::any GetNativeWindow() const override;

        void SetMode(const Tbx::WindowMode& mode) override;

    private:
        std::string _title;
        Tbx::Size _size;
        std::shared_ptr<Tbx::Camera> _camera;
        GLFWwindow* _glfwWindow = nullptr;

        void SetupCallbacks();
        void OnKeyPressed(int key, int scancode, int action, int mods) const;
        void OnMouseButtonPressed(int button, int action, int mods) const;
        void OnMouseScrolled(double offsetX, double offsetY) const;
        void OnMouseMoved(double posX, double posY) const;
        void OnWindowClosed() const;
        void OnSizeChanged() const;
    };
}
