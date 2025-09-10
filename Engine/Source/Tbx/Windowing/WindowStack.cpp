#include "Tbx/PCH.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/PluginAPI/PluginServer.h"

namespace Tbx
{
    WindowStack::WindowStack()
    {
        _windowFactory = PluginServer::Get<IWindowFactoryPlugin>();
        TBX_VALIDATE_WEAK_PTR(_windowFactory, "Window factory plugin not found!");
    }

    WindowStack::~WindowStack()
    {
        Clear();
    }

    void WindowStack::Push(std::shared_ptr<IWindow> window)
    {
        _windows.push_back(window);
    }

    Uid WindowStack::Emplace(const std::string& name, const Size& size, const WindowMode& mode)
    {
        std::shared_ptr<IWindow> window = nullptr;

        if (_windowFactory.expired() || !_windowFactory.lock())
        {
            TBX_ASSERT(false, "Window factory plugin not found!");
            return Invalid::Uid;
        }

        // Will suppress the window opened event in this scope
        // as we don't want it to happen until we have pushed the window back into the window stack
        {
            EventSuppressor suppressor;

            // Create and open the window and push it into our stack
            window = _windowFactory.lock()->Create(name, size, mode);
            _windows.push_back(window);
        }

        // Ensure our window was created
        TBX_ASSERT(window, "Failed to create window!");
        if (window == nullptr)
        {
            return -1;
        }

        // Send the window opened event
        auto windowOpenedEvt = WindowOpenedEvent(window);
        EventCoordinator::Send(windowOpenedEvt);

        return window->GetId();
    }

    bool WindowStack::Contains(const Uid& id) const
    {
        auto window = std::find_if(_windows.begin(), _windows.end(), [id](std::shared_ptr<IWindow> window)
        {
            return window->GetId() == id;
        });
        return window != _windows.end();
    }

    std::shared_ptr<IWindow> WindowStack::Get(const Uid& id) const
    {
        auto window = std::find_if(_windows.begin(), _windows.end(), [id](std::shared_ptr<IWindow> window)
        {
            return window->GetId() == id;
        });
        TBX_ASSERT(window != _windows.end(), "Window with the id {} does not exist!", id.ToString());
        return *window;
    }

    const std::vector<std::shared_ptr<IWindow>>& WindowStack::GetAll()
    {
        return _windows;
    }

    void WindowStack::Remove(const Uid& id)
    {
        if (_windows.empty()) return;

        auto window = std::find_if(_windows.begin(), _windows.end(), [id](std::shared_ptr<IWindow> window)
        {
            return window->GetId() == id;
        });
        TBX_ASSERT(window != _windows.end(), "Window with the id {} does not exist!", id.ToString());
        _windows.erase(window);
    }

    void WindowStack::Clear()
    {
        _windows.clear();
    }
}
