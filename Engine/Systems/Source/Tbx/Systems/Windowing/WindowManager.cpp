#include "Tbx/Systems/PCH.h"
#include "Tbx/Systems/Windowing/WindowManager.h"
#include "Tbx/Systems/Events/EventCoordinator.h"
#include "Tbx/Systems/Debug/Debugging.h"

namespace Tbx
{
    std::map<UID, std::shared_ptr<IWindow>> WindowManager::_windows;
    std::vector<UID> WindowManager::_windowsToCloseOnNextUpdate;

    UID WindowManager::_mainWindowId = -1;
    UID WindowManager::_focusedWindowId = -1;
    UID WindowManager::_windowCloseEventId = -1;
    UID WindowManager::_windowFocusChangedEventId = -1;

    void WindowManager::SetContext()
    {
        _windowCloseEventId = EventCoordinator::Subscribe<WindowClosedEvent>(OnWindowClose);
        _windowFocusChangedEventId = EventCoordinator::Subscribe<WindowFocusChangedEvent>(OnWindowFocusChanged);
    }

    void WindowManager::Shutdown()
    {
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_windowCloseEventId);
        EventCoordinator::Unsubscribe<WindowFocusChangedEvent>(_windowFocusChangedEventId);

        CloseAllWindows();
    }

    void WindowManager::DrawFrame()
    {
        if (_windows.empty()) return;

        // Update all windows
        for (const auto& [id, window] : _windows)
        {
            window->DrawFrame();
        }

        // Close windows that are mark for close
        for (const auto& id : _windowsToCloseOnNextUpdate)
        {
            CloseWindow(id);
        }
        _windowsToCloseOnNextUpdate.clear();
    }

    UID WindowManager::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        auto openRequest = OpenNewWindowRequest(name, mode, size);
        EventCoordinator::Send<OpenNewWindowRequest>(openRequest);
        auto window = openRequest.GetResult();

        TBX_ASSERT(openRequest.IsHandled, "Failed to open new window with the name {}!", name);
        TBX_VALIDATE_PTR(window, "No result returned when opening new window with the name {}!", name);

        if (_windows.empty())
        {
            _mainWindowId = window->Id;
        }

        _windows[window->Id] = window;
        window->Focus();

        auto windowOpenedEvent = WindowOpenedEvent(window->Id);
        EventCoordinator::Send<WindowOpenedEvent>(windowOpenedEvent);

        return window->Id;
    }

    std::shared_ptr<IWindow> WindowManager::GetMainWindow()
    {
        return GetWindow(_mainWindowId);
    }

    std::shared_ptr<IWindow> WindowManager::GetWindow(const UID& id)
    {
        TBX_ASSERT(_windows.find(id) != _windows.end(), "Window with the id {} does not exist!", id.ToString());
        return _windows[id];
    }

    std::shared_ptr<IWindow> WindowManager::GetFocusedWindow()
    {
        return GetWindow(_focusedWindowId);
    }

    std::vector<std::shared_ptr<IWindow>> WindowManager::GetAllWindows()
    {
        std::vector<std::shared_ptr<IWindow>> windows;
        for (const auto& [id, window] : _windows)
        {
            windows.push_back(window);
        }
        return windows;
    }

    void WindowManager::CloseWindow(const UID& id)
    {
        if (_windows.empty()) return;

        TBX_ASSERT(_windows.find(id) != _windows.end(), "Window with the id {} does not exist!", id.ToString());
        _windows.erase(id);
    }

    void WindowManager::CloseAllWindows()
    {
        _windows.clear();
    }

    void WindowManager::OnWindowClose(const WindowClosedEvent& e)
    {
        _windowsToCloseOnNextUpdate.push_back(e.GetWindowId());
    }

    void WindowManager::OnWindowFocusChanged(const WindowFocusChangedEvent& e)
    {
        if (e.IsFocused())
        {
            _focusedWindowId = e.GetWindowId();
        }
    }
}
