#include "Tbx/App/PCH.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include "Tbx/App/Events/WindowEvents.h"
#include <Tbx/Core/Events/EventDispatcher.h>
#include <Tbx/Core/Debug/DebugAPI.h>

namespace Tbx
{
    std::map<UID, std::shared_ptr<IWindow>> WindowManager::_windows;
    UID WindowManager::_maindWindowId = -1;

    UID WindowManager::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        auto event = OpenNewWindowRequestEvent(name, mode, size);
        EventDispatcher::Send<OpenNewWindowRequestEvent>(event);
        auto window = event.GetResult();

        TBX_ASSERT(event.Handled, "Failed to open new window with the name {}!", name);
        TBX_VALIDATE_PTR(window, "No result returned when opening new window with the name {}!", name);

        if (_windows.empty())
        {
            _maindWindowId = window->GetId();
        }

        _windows[window->GetId()] = window;
        return window->GetId();
    }

    std::weak_ptr<IWindow> WindowManager::GetMainWindow()
    {
        return GetWindow(_maindWindowId);
    }

    std::weak_ptr<IWindow> WindowManager::GetWindow(const UID& id)
    {
        TBX_ASSERT(_windows.find(id) != _windows.end(), "Window with the id {} does not exist!", id.ToString());
        return _windows[id];
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
        TBX_ASSERT(_windows.find(id) != _windows.end(), "Window with the id {} does not exist!", id.ToString());
        _windows.erase(id);
    }

    void WindowManager::CloseAllWindows()
    {
        _windows.clear();
    }
}
