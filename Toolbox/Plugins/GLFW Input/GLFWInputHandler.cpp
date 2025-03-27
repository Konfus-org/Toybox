#include "GLFWInputHandler.h"

namespace GLFWInput
{
    void GLFWInputHandler::SetContext(const std::weak_ptr<Tbx::IWindow>& windowToListenTo)
    {
        _windowToListenTo = std::any_cast<GLFWwindow*>(windowToListenTo.lock()->GetNativeWindow());
    }

    bool GLFWInputHandler::IsGamepadButtonDown(const int button, int id)
    {
        return GetGamepadButtonState(0, button) == GLFW_PRESS;
    }

    bool GLFWInputHandler::IsGamepadButtonUp(const int button, int id)
    {
        return GetGamepadButtonState(0, button) == GLFW_RELEASE;
    }

    bool GLFWInputHandler::IsGamepadButtonHeld(const int button, int id)
    {
        return GetGamepadButtonState(0, button) == GLFW_REPEAT;
    }

    bool GLFWInputHandler::IsKeyDown(const int keyCode)
    {
        return GetKeyState(keyCode) == GLFW_PRESS;
    }

    bool GLFWInputHandler::IsKeyUp(const int keyCode)
    {
        return GetKeyState(keyCode) == GLFW_RELEASE;
    }

    bool GLFWInputHandler::IsKeyHeld(const int keyCode)
    {
        return GetKeyState(keyCode) == GLFW_REPEAT;
    }

    bool GLFWInputHandler::IsMouseButtonDown(const int button)
    {
        return GetMouseButtonState(button) == GLFW_PRESS;
    }

    bool GLFWInputHandler::IsMouseButtonUp(const int button)
    {
        return GetMouseButtonState(button) == GLFW_RELEASE;
    }

    bool GLFWInputHandler::IsMouseButtonHeld(const int button)
    {
        return GetMouseButtonState(button) == GLFW_REPEAT;
    }

    Tbx::Vector2 GLFWInputHandler::GetMousePosition()
    {
        double xPos;
        double yPos;
        glfwGetCursorPos(_windowToListenTo, &xPos, &yPos);
        return Tbx::Vector2((float)xPos, (float)yPos);
    }

    int GLFWInputHandler::GetKeyState(int keyCode)
    {
        auto state = glfwGetKey(_windowToListenTo, keyCode);
        return state;
    }

    int GLFWInputHandler::GetMouseButtonState(int button)
    {
        auto state = glfwGetMouseButton(_windowToListenTo, button);
        return state;
    }

    int GLFWInputHandler::GetGamepadButtonState(int button, int id)
    {
        int numberOfPressedButtons;
        const unsigned char* buttons = glfwGetJoystickButtons(id, &numberOfPressedButtons);
        return buttons[button];
    }
}
