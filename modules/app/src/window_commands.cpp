#include "tbx/app/window_commands.h"
#include <utility>

namespace tbx
{
    CreateWindowCommand::CreateWindowCommand(WindowDescription desc)
        : description(std::move(desc))
    {
    }

    OpenWindowCommand::OpenWindowCommand(Window* window_ptr, WindowDescription desc)
        : window(window_ptr)
        , description(std::move(desc))
    {
    }

    QueryWindowDescriptionCommand::QueryWindowDescriptionCommand(Window* window_ptr)
        : window(window_ptr)
    {
    }

    ApplyWindowDescriptionCommand::ApplyWindowDescriptionCommand(
        Window* window_ptr,
        WindowDescription desc)
        : window(window_ptr)
        , description(std::move(desc))
    {
    }

    CloseWindowCommand::CloseWindowCommand(Window* window_ptr)
        : window(window_ptr)
    {
    }
}
