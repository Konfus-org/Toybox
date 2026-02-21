#include "tbx/graphics/window.h"
#include "tbx/debugging/macros.h"
#include "tbx/messages/dispatcher.h"

namespace tbx
{
    Window::Window(
        IMessageDispatcher& dispatcher,
        std::string title,
        Size size,
        WindowMode mode,
        bool open_on_creation)
        : title(&dispatcher, this, &Window::title, title)
        , size(&dispatcher, this, &Window::size, size)
        , mode(&dispatcher, this, &Window::mode, mode)
        , is_open(&dispatcher, this, &Window::is_open, open_on_creation)
        , native_handle(&dispatcher, this, &Window::native_handle, nullptr)
    {
    }

    Window::~Window() noexcept
    {
        native_handle = nullptr;
        is_open = false;
    }
}
