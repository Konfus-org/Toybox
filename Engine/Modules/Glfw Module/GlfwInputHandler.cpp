#include "GlfwInputHandler.h"
#include <Core.h>

namespace GlfwInput
{
    GlfwInputHandler::GlfwInputHandler(std::any mainWindow)
    {
        _mainWindow = std::any_cast<GLFWwindow*>(mainWindow);
    }

    bool GlfwInputHandler::IsGamepadButtonDown(const int button, int id)
    {
        return GetGamepadButtonState(0, button) == GLFW_PRESS;
    }

    bool GlfwInputHandler::IsGamepadButtonUp(const int button, int id)
    {
        return GetGamepadButtonState(0, button) == GLFW_RELEASE;
    }

    bool GlfwInputHandler::IsGamepadButtonHeld(const int button, int id)
    {
        return GetGamepadButtonState(0, button) == GLFW_REPEAT;
    }

    bool GlfwInputHandler::IsKeyDown(const int keyCode)
    {
        return GetKeyState(keyCode) == GLFW_PRESS;
    }

    bool GlfwInputHandler::IsKeyUp(const int keyCode)
    {
        return GetKeyState(keyCode) == GLFW_RELEASE;
    }

    bool GlfwInputHandler::IsKeyHeld(const int keyCode)
    {
        return GetKeyState(keyCode) == GLFW_REPEAT;
    }

    bool GlfwInputHandler::IsMouseButtonDown(const int button)
    {
        return GetMouseButtonState(button) == GLFW_PRESS;
    }

    bool GlfwInputHandler::IsMouseButtonUp(const int button)
    {
        return GetMouseButtonState(button) == GLFW_RELEASE;
    }

    bool GlfwInputHandler::IsMouseButtonHeld(const int button)
    {
        return GetMouseButtonState(button) == GLFW_REPEAT;
    }

    Toybox::Vector2 GlfwInputHandler::GetMousePosition()
    {
        double xPos;
        double yPos;
        glfwGetCursorPos(_mainWindow, &xPos, &yPos);
        return Toybox::Vector2((float)xPos, (float)yPos);
    }

    int GlfwInputHandler::GetKeyState(int keyCode)
    {
        auto state = glfwGetKey(_mainWindow, keyCode);
        return state;
    }

    int GlfwInputHandler::GetMouseButtonState(int button)
    {
        auto state = glfwGetMouseButton(_mainWindow, button);
        return state;
    }

    int GlfwInputHandler::GetGamepadButtonState(int button, int id)
    {
        int numberOfPressedButtons;
        const unsigned char* buttons = glfwGetJoystickButtons(id, &numberOfPressedButtons);
        return buttons[button];
    }

}
