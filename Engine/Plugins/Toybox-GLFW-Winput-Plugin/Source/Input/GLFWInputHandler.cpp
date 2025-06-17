#include "GLFWInputHandler.h"
#include "Tbx/Systems/Debug/Debugging.h"
#include <any>

namespace GLFWInput
{
    void GLFWInputHandler::SetContext(const std::shared_ptr<Tbx::IWindow>& windowToListenTo)
    {
        auto nativeWindow = windowToListenTo->GetNativeWindow();
        _windowToListenTo = static_cast<GLFWwindow*>(nativeWindow);
    }

    bool GLFWInputHandler::IsGamepadButtonDown(const int button, int id) const
    {
        return GetGamepadButtonState(0, button) == GLFW_PRESS;
    }

    bool GLFWInputHandler::IsGamepadButtonUp(const int button, int id) const
    {
        return GetGamepadButtonState(0, button) == GLFW_RELEASE;
    }

    bool GLFWInputHandler::IsGamepadButtonHeld(const int button, int id) const
    {
        return GetGamepadButtonState(0, button) == GLFW_REPEAT;
    }

    bool GLFWInputHandler::IsKeyDown(const int keyCode) const
    {
        return GetKeyState(keyCode) == GLFW_PRESS;
    }

    bool GLFWInputHandler::IsKeyUp(const int keyCode) const
    {
        return GetKeyState(keyCode) == GLFW_RELEASE;
    }

    bool GLFWInputHandler::IsKeyHeld(const int keyCode) const
    {
        return GetKeyState(keyCode) == GLFW_REPEAT;
    }

    bool GLFWInputHandler::IsMouseButtonDown(const int button) const
    {
        return GetMouseButtonState(button) == GLFW_PRESS;
    }

    bool GLFWInputHandler::IsMouseButtonUp(const int button) const
    {
        return GetMouseButtonState(button) == GLFW_RELEASE;
    }

    bool GLFWInputHandler::IsMouseButtonHeld(const int button) const
    {
        return GetMouseButtonState(button) == GLFW_REPEAT;
    }

    Tbx::Vector2 GLFWInputHandler::GetMousePosition() const
    {
        TBX_VALIDATE_PTR(_windowToListenTo, "Cannot get mouse position, there is no context set (window to ask for input)!");
        if (_windowToListenTo == nullptr) return {};

        double xPos;
        double yPos;
        glfwGetCursorPos(_windowToListenTo, &xPos, &yPos);
        return {static_cast<float>(xPos), static_cast<float>(yPos)};
    }

    int GLFWInputHandler::GetKeyState(int keyCode) const
    {
        TBX_VALIDATE_PTR(_windowToListenTo, "Cannot get key state, there is no context set (window to ask for input)!");
        if (_windowToListenTo == nullptr) return -1;

        auto state = glfwGetKey(_windowToListenTo, keyCode);
        return state;
    }

    int GLFWInputHandler::GetMouseButtonState(int button) const
    {
        TBX_VALIDATE_PTR(_windowToListenTo, "Cannot get mouse button state, there is no context set (window to ask for input)!");
        if (_windowToListenTo == nullptr) return -1;

        auto state = glfwGetMouseButton(_windowToListenTo, button);
        return state;
    }

    int GLFWInputHandler::GetGamepadButtonState(int button, int id) const
    {
        TBX_VALIDATE_PTR(_windowToListenTo, "Cannot get gamepad button state, there is no context set (window to ask for input)!");
        if (_windowToListenTo == nullptr) return -1;

        int numberOfPressedButtons;
        const unsigned char* buttons = glfwGetJoystickButtons(id, &numberOfPressedButtons);
        return buttons[button];
    }
}
