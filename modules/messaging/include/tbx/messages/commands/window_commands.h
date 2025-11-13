#pragma once
#include "tbx/messages/commands/command.h"
#include "tbx/messages/window_description.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    class Window;

    // Command requesting a new platform window.
    struct TBX_API CreateWindowCommand : public Command
    {
        CreateWindowCommand() = default;
        CreateWindowCommand(WindowDescription desc);

        WindowDescription description = {};
    };

    // Command emitted when a window is considered open.
    struct TBX_API OpenWindowCommand : public Command
    {
        OpenWindowCommand() = default;
        OpenWindowCommand(Window* window_ptr, WindowDescription desc);

        // Non-owning pointer to the window that was opened.
        Window* window = nullptr;
        WindowDescription description = {};
    };

    // Command requesting the latest window state from the platform backend.
    struct TBX_API QueryWindowDescriptionCommand : public Command
    {
        QueryWindowDescriptionCommand() = default;
        QueryWindowDescriptionCommand(Window* window_ptr);

        // Non-owning pointer to the window being queried.
        Window* window = nullptr;
    };

    // Command requesting the platform backend to apply a new description.
    struct TBX_API ApplyWindowDescriptionCommand : public Command
    {
        ApplyWindowDescriptionCommand() = default;
        ApplyWindowDescriptionCommand(Window* window_ptr, WindowDescription desc);

        // Non-owning pointer to the window to be updated.
        Window* window = nullptr;
        WindowDescription description = {};
    };

    // Command requesting the platform backend to close a window.
    struct TBX_API CloseWindowCommand : public Command
    {
        CloseWindowCommand() = default;
        CloseWindowCommand(Window* window_ptr);

        // Non-owning pointer to the window being closed.
        Window* window = nullptr;
    };
}
