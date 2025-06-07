#pragma once
#include <Tbx/Application/Input/IInputHandler.h>
#include <GLFW/glfw3.h>
#include <memory>

namespace GLFWInput
{
    class GLFWInputHandler : public Tbx::IInputHandler
    {
    public:
        GLFWInputHandler() = default;
        ~GLFWInputHandler() override = default;

        void SetContext(const std::weak_ptr<Tbx::IWindow>& windowToListenTo) override;

        bool IsGamepadButtonDown(const int id, const int button) const override;
        bool IsGamepadButtonUp(const int id, const int button) const override;
        bool IsGamepadButtonHeld(const int id, const int button) const override;

        bool IsKeyDown(const int keyCode) const override;
        bool IsKeyUp(const int keyCode) const override;
        bool IsKeyHeld(const int keyCode) const override;

        bool IsMouseButtonDown(const int button) const override;
        bool IsMouseButtonUp(const int button) const override;
        bool IsMouseButtonHeld(const int button) const override;
        Tbx::Vector2 GetMousePosition() const override;

    private:
        GLFWwindow* _windowToListenTo = nullptr;

        int GetKeyState(int keyCode) const;
        int GetMouseButtonState(int button) const;
        int GetGamepadButtonState(int button, int id) const;
    };
}

