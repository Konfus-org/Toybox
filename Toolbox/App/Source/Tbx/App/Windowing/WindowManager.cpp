#include "Tbx/App/PCH.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include <Tbx/Core/Events/EventDispatcher.h>
#include <Tbx/Core/Debug/DebugAPI.h>

namespace Tbx
{
    std::map<UID, std::shared_ptr<IWindow>> WindowManager::_windows;
    UID WindowManager::_mainWindowId = -1;
    UID WindowManager::_focusedWindowId = -1;
    UID WindowManager::_appUpdatedEventId = -1;
    UID WindowManager::_windowCloseEventId = -1;
    UID WindowManager::_windowFocusChangedEventId = -1;

    void WindowManager::Initialize()
    {
        _appUpdatedEventId = 
            EventDispatcher::Subscribe<AppUpdatedEvent>(TBX_BIND_STATIC_CALLBACK(OnAppUpdated));
        _windowCloseEventId =
            EventDispatcher::Subscribe<WindowClosedEvent>(TBX_BIND_STATIC_CALLBACK(OnWindowClose));
        _windowFocusChangedEventId =
            EventDispatcher::Subscribe<WindowFocusChangedEvent>(TBX_BIND_STATIC_CALLBACK(OnWindowFocusChanged));
    }

    void WindowManager::Shutdown()
    {
        EventDispatcher::Unsubscribe(_appUpdatedEventId);
        EventDispatcher::Unsubscribe(_windowCloseEventId);
        EventDispatcher::Unsubscribe(_windowFocusChangedEventId);

        CloseAllWindows();
    }

    UID WindowManager::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        auto event = OpenNewWindowRequestEvent(name, mode, size);
        EventDispatcher::Dispatch<OpenNewWindowRequestEvent>(event);
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
        if (_windows.empty())
        {
            return;
        }

        TBX_ASSERT(_windows.find(id) != _windows.end(), "Window with the id {} does not exist!", id.ToString());
        _windows.erase(id);
    }

    void WindowManager::CloseAllWindows()
    {
        _windows.clear();
    }

    void WindowManager::OnAppUpdated(const AppUpdatedEvent& e)
    {
        for (const auto& [id, window] : _windows)
        {
            window->Update();
        }
    }

    void WindowManager::OnWindowClose(const WindowClosedEvent& e)
    {
        CloseWindow(e.GetWindowId());
    }

    void WindowManager::OnWindowFocusChanged(const WindowFocusChangedEvent& e)
    {
        if (e.IsFocused())
        {
            _focusedWindowId = e.GetWindowId();
        }
    }
}
