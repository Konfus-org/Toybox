#include "runtime_state.h"
#include "tbx/platform/window.h"
#include <utility>

// The platform-agnostic window API over the running runtime — opening a window and the fluent
// WindowHandle. The OS-specific work (materializing surfaces, applying data) lives behind the
// backend seam in platform/<backend>/; this file only reads/writes the plain Window data the
// backend later applies. Main-thread only (resolves through tbx::internal::get_runtime()).
namespace tbx
{
    //// WINDOW HANDLE ////

    WindowHandle::WindowHandle(const size index)
        : _index(index)
    {
    }

    std::optional<std::reference_wrapper<Window>> WindowHandle::resolve() const
    {
        if (!internal::has_runtime())
            return {};
        auto& open_windows = internal::get_runtime().windows.open_windows;
        if (_index >= open_windows.size())
            return {};
        return open_windows[_index];
    }

    WindowHandle& WindowHandle::set_mode(const WindowMode mode)
    {
        if (const auto window = resolve())
            window->get().mode = mode;
        return *this;
    }

    WindowHandle& WindowHandle::set_title(std::string title)
    {
        if (const auto window = resolve())
            window->get().title = std::move(title);
        return *this;
    }

    WindowHandle& WindowHandle::set_size(const uint32 width, const uint32 height)
    {
        if (const auto window = resolve())
        {
            window->get().width = width;
            window->get().height = height;
        }
        return *this;
    }

    WindowHandle& WindowHandle::set_icon(AssetHandle<Texture> icon)
    {
        if (const auto window = resolve())
            window->get().icon = std::move(icon);
        return *this;
    }

    bool WindowHandle::is_valid() const
    {
        const auto window = resolve();
        return window && window->get().status == WindowStatus::OPEN;
    }

    WindowMode WindowHandle::get_mode() const
    {
        const auto window = resolve();
        return window ? window->get().mode : WindowMode::WINDOWED;
    }

    const std::string& WindowHandle::get_title() const
    {
        static const std::string EMPTY = {};
        const auto window = resolve();
        return window ? window->get().title : EMPTY;
    }

    uint32 WindowHandle::get_width() const
    {
        const auto window = resolve();
        return window ? window->get().width : 0;
    }

    uint32 WindowHandle::get_height() const
    {
        const auto window = resolve();
        return window ? window->get().height : 0;
    }

    //// WINDOWS ////

    WindowHandle open(Window window)
    {
        auto& open_windows = internal::get_runtime().windows.open_windows;
        open_windows.push_back(std::move(window));
        return WindowHandle(open_windows.size() - 1);
    }

    const std::vector<Window>& get_open_windows()
    {
        return internal::get_runtime().windows.open_windows;
    }
}
