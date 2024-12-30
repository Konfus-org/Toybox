#pragma once
#include <Core.h>
#include <GLFW/glfw3.h>

namespace GLFWInput
{
    class GLFWInputHandler : public Toybox::IInputHandler
    {
    public:
        void SetContext(const std::weak_ptr<Toybox::IWindow>& windowToListenTo) override;

        bool IsGamepadButtonDown(const int id, const int button) override;
        bool IsGamepadButtonUp(const int id, const int button) override;
        bool IsGamepadButtonHeld(const int id, const int button) override;

        bool IsKeyDown(const int keyCode) override;
        bool IsKeyUp(const int keyCode) override;
        bool IsKeyHeld(const int keyCode) override;

        bool IsMouseButtonDown(const int button) override;
        bool IsMouseButtonUp(const int button) override;
        bool IsMouseButtonHeld(const int button) override;
        Toybox::Vector2 GetMousePosition() override;

    private:
        GLFWwindow* _windowToListenTo = nullptr;

        int GetKeyState(int keyCode);
        int GetMouseButtonState(int button);
        int GetGamepadButtonState(int button, int id);
    };
}

