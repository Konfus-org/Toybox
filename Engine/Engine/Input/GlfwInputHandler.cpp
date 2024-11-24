#include "tbxpch.h"
#include "Application/App.h"
#include "GlfwInputHandler.h"
#include <GLFW/glfw3.h>

namespace Toybox::Input
{
    int GetKeyState(int inputCode)
    {
        auto* app = Application::App::GetInstance();
        GLFWwindow* appMainWindow = std::any_cast<GLFWwindow*>(app->GetMainWindow()->GetNativeWindow());
        auto state = glfwGetKey(appMainWindow, inputCode);
        return state;
        return false;
    }

    // TODO: implement gamepad input
    int GetGamepadButtonState(int inputCode, int id)
    {
        //auto state = glfwGetJoystickButtons(id, inputCode);
        return 0;
    }

    bool GlfwInputHandler::IsGamepadButtonDown(const int inputCode)
    {
        return GetGamepadButtonState(0, inputCode);
    }

    bool GlfwInputHandler::IsGamepadButtonUp(const int inputCode)
    {
        return GetGamepadButtonState(0, inputCode);
    }

    bool GlfwInputHandler::IsGamepadButtonHeld(const int inputCode)
    {
        return GetGamepadButtonState(0, inputCode);
    }

    bool GlfwInputHandler::IsKeyDown(const int inputCode)
    {
        return GetKeyState(inputCode) == GLFW_PRESS;
    }

    bool GlfwInputHandler::IsKeyUp(const int inputCode)
    {
        return GetKeyState(inputCode) == GLFW_RELEASE;
    }

    bool GlfwInputHandler::IsKeyHeld(const int inputCode)
    {
        return GetKeyState(inputCode) == GLFW_REPEAT;
    }
}
