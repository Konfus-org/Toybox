#include "Tbx/PCH.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Debug/Debugging.h"
#include "Tbx/Plugin API/PluginServer.h"

namespace Tbx
{
    WindowStack::WindowStack()
    {
        _windowFactory = PluginServer::GetPlugin<IWindowFactoryPlugin>();
        TBX_VALIDATE_PTR(_windowFactory, "Window factory plugin not found!");
    }

    WindowStack::~WindowStack()
    {
        Clear();
    }

    UID WindowStack::Push(const std::string& name, const WindowMode& mode, const Size& size)
    {
        auto window = _windowFactory->Create();

        TBX_VALIDATE_PTR(window, "No result returned when opening new window with the name {}!", name);
        if (window == nullptr) return -1;

        _windows.push_back(window);

        window->SetTitle(name);
        window->SetSize(size);
        window->Open(mode);
        window->Focus();

        return window->GetId();
    }

    std::shared_ptr<IWindow> WindowStack::Get(const UID& id) const
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

    void WindowStack::Remove(const UID& id)
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
