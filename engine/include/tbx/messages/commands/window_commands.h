#pragma once
#include "tbx/messages/commands/command.h"
#include "tbx/os/window.h"
#include <utility>

namespace tbx
{
    // Command requesting a new platform window.
    class CreateWindowCommand : public Command
    {
    public:
        CreateWindowCommand() = default;
        CreateWindowCommand(WindowDescription desc)
            : description(std::move(desc))
        {
        }

        WindowDescription description = {};
    };

    // Command requesting the latest window state from the platform backend.
    class QueryWindowDescriptionCommand : public Command
    {
    public:
        QueryWindowDescriptionCommand() = default;
        QueryWindowDescriptionCommand(Window& window_ref)
            : window(&window_ref)
        {
        }

        Window* window = nullptr;
    };

    // Command requesting the platform backend to apply a new description.
    class ApplyWindowDescriptionCommand : public Command
    {
    public:
        ApplyWindowDescriptionCommand() = default;
        ApplyWindowDescriptionCommand(Window& window_ref, WindowDescription desc)
            : window(&window_ref)
            , description(std::move(desc))
        {
        }

        Window* window = nullptr;
        WindowDescription description = {};
    };
}
