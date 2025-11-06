#pragma once
#include "tbx/messages/commands/command.h"
#include "tbx/os/window.h"
#include "tbx/tbx_api.h"

namespace tbx
{
    // Command requesting a new platform window.
    class TBX_API CreateWindowCommand : public Command
    {
      public:
        CreateWindowCommand() = default;
        explicit CreateWindowCommand(WindowDescription desc);

        WindowDescription description = {};
    };

    // Command emitted when a window is considered open.
    class TBX_API OpenWindowCommand : public Command
    {
      public:
        OpenWindowCommand() = default;
        OpenWindowCommand(Window* window_ptr, WindowDescription desc);

        // Non-owning pointer to the window that was opened.
        Window* window = nullptr;
        WindowDescription description = {};
    };

    // Command requesting the latest window state from the platform backend.
    class TBX_API QueryWindowDescriptionCommand : public Command
    {
       public:
        QueryWindowDescriptionCommand() = default;
        explicit QueryWindowDescriptionCommand(Window* window_ptr);

        // Non-owning pointer to the window being queried.
        Window* window = nullptr;
    };

    // Command requesting the platform backend to apply a new description.
    class TBX_API ApplyWindowDescriptionCommand : public Command
    {
       public:
        ApplyWindowDescriptionCommand() = default;
        ApplyWindowDescriptionCommand(Window* window_ptr, WindowDescription desc);

        // Non-owning pointer to the window to be updated.
        Window* window = nullptr;
        WindowDescription description = {};
    };

    // Command requesting the platform backend to close a window.
    class TBX_API CloseWindowCommand : public Command
    {
       public:
        CloseWindowCommand() = default;
        explicit CloseWindowCommand(Window* window_ptr);

        // Non-owning pointer to the window being closed.
        Window* window = nullptr;
    };
}
