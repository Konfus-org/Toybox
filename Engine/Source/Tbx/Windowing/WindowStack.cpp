#include "Tbx/PCH.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    WindowStack::~WindowStack()
    {
        Clear();
    }

    void WindowStack::Push(Tbx::Ref<IWindow> window)
    {
        _windows.push_back(window);
    }

    bool WindowStack::Contains(const Uid& id) const
    {
        auto window = std::find_if(_windows.begin(), _windows.end(), [id](Tbx::Ref<IWindow> window)
        {
            return window->GetId() == id;
        });
        return window != _windows.end();
    }

    Tbx::Ref<IWindow> WindowStack::Get(const Uid& id) const
    {
        auto window = std::find_if(_windows.begin(), _windows.end(), [id](Tbx::Ref<IWindow> window)
        {
            return window->GetId() == id;
        });
        TBX_ASSERT(window != _windows.end(), "Window with the id {} does not exist!", id.ToString());
        return *window;
    }

    const std::vector<Tbx::Ref<IWindow>>& WindowStack::GetAll() const
    {
        return _windows;
    }

    void WindowStack::Remove(const Uid& id)
    {
        if (_windows.empty()) return;

        auto window = std::find_if(_windows.begin(), _windows.end(), [id](Tbx::Ref<IWindow> window)
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
