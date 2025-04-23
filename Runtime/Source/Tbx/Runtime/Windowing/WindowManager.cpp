#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/Windowing/WindowManager.h"
#include <Tbx/Core/Events/EventCoordinator.h>
#include <Tbx/Core/Debug/DebugAPI.h>

namespace Tbx
{
    std::map<UID, std::shared_ptr<IWindow>> WindowManager::_windows;
    std::vector<UID> WindowManager::_windowsToCloseOnNextUpdate;
    UID WindowManager::_mainWindowId = -1;
    UID WindowManager::_focusedWindowId = -1;
    UID WindowManager::_appUpdatedEventId = -1;
    UID WindowManager::_windowCloseEventId = -1;
    UID WindowManager::_windowFocusChangedEventId = -1;

    bool WindowManager::IsOverlay()
    {
        return false;
    }

    void WindowManager::OnAttach()
    {
        _windowCloseEventId =
            EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_STATIC_FN(OnWindowClose));
        _windowFocusChangedEventId =
            EventCoordinator::Subscribe<WindowFocusChangedEvent>(TBX_BIND_STATIC_FN(OnWindowFocusChanged));
    }

    void WindowManager::OnDetach()
    {
        EventCoordinator::Unsubscribe<AppUpdatedEvent>(_appUpdatedEventId);
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_windowCloseEventId);
        EventCoordinator::Unsubscribe<WindowFocusChangedEvent>(_windowFocusChangedEventId);

        CloseAllWindows();
    }

    void WindowManager::OnUpdate()
    {
        if (_windows.empty()) return;

        // Update all windows
        for (const auto& [id, window] : _windows)
        {
            window->Update();
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
        auto event = OpenNewWindowRequest(name, mode, size);
        EventCoordinator::Send<OpenNewWindowRequest>(event);
        auto window = event.GetResult();

        TBX_ASSERT(event.IsHandled, "Failed to open new window with the name {}!", name);
        TBX_VALIDATE_PTR(window, "No result returned when opening new window with the name {}!", name);

        if (_windows.empty())
        {
            _mainWindowId = window->GetId();
        }

        _windows[window->GetId()] = window;
        return window->GetId();
    }

    std::weak_ptr<IWindow> WindowManager::GetMainWindow()
    {
        return GetWindow(_mainWindowId);
    }

    std::weak_ptr<IWindow> WindowManager::GetWindow(const UID& id)
    {
        TBX_ASSERT(_windows.find(id) != _windows.end(), "Window with the id {} does not exist!", id.ToString());
        return _windows[id];
    }

    std::weak_ptr<IWindow> WindowManager::GetFocusedWindow()
    {
        return GetWindow(_focusedWindowId);
    }

    std::vector<std::weak_ptr<IWindow>> WindowManager::GetAllWindows()
    {
        std::vector<std::weak_ptr<IWindow>> windows;
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
