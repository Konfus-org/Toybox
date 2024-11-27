#include "GlfwInputHandler.h"
#include <GLFW/glfw3.h>
#include <Toybox.h>

namespace GlfwInput
{
    GLFWwindow* GetAppMainGlfwWindow()
    {
        auto* app = Toybox::Application::App::GetInstance();
        GLFWwindow* appMainWindow = std::any_cast<GLFWwindow*>(app->GetMainWindow()->GetNativeWindow());
        return appMainWindow;
    }

    int GetKeyState(int keyCode)
    {
        auto state = glfwGetKey(GetAppMainGlfwWindow(), keyCode);
        return state;
    }

    int GetMouseButtonState(int button)
    {
        auto state = glfwGetMouseButton(GetAppMainGlfwWindow(), button);
        return state;
    }

    int GetGamepadButtonState(int button, int id)
    {
        int numberOfPressedButtons;
        const unsigned char* buttons = glfwGetJoystickButtons(id, &numberOfPressedButtons);
        return buttons[button];
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

    Toybox::Math::Vector2 GlfwInputHandler::GetMousePosition()
    {
        double xPos;
        double yPos;
        glfwGetCursorPos(GetAppMainGlfwWindow(), &xPos, &yPos);
        return Toybox::Math::Vector2((float)xPos, (float)yPos);
    }
}
