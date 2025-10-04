#include "Tbx/PCH.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Debug/Tracers.h"

namespace Tbx
{
    WindowManager::WindowManager(
        Ref<IWindowFactory> windowFactory,
        Ref<EventBus> eventBus)
    {
        _windowFactory = windowFactory;
        TBX_ASSERT(_windowFactory, "Window manager given invalid window factory!");
        _eventBus = eventBus;
        TBX_ASSERT(_eventBus, "Window manager given invalid event bus!");
    }

    WindowManager::~WindowManager()
    {
        CloseAllWindows();
    }

    void WindowManager::UpdateWindows() const
    {
        for (const auto& window : _stack)
        {
            window->Update();
        }
    }

    const std::vector<Ref<Window>>& WindowManager::GetAllWindows() const
    {
        return _stack.GetAll();
    }

    Ref<Window> WindowManager::GetMainWindow() const
    {
        return GetWindow(_mainWindowId);
    }

    Ref<Window> WindowManager::GetWindow(const Uid& id) const
    {
        return _stack.Get(id);
    }

    Uid WindowManager::OpenWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        Ref<Window> window = _windowFactory->Create(name, size, mode, _eventBus);
        TBX_ASSERT(window, "Failed to create window!");
        if (_mainWindowId == Uid::Invalid)
        {
            _mainWindowId = window->Id;
        }
        
        _stack.Push(window);
        window->Open();
        window->Focus();

        return window->Id;
    }

    void WindowManager::CloseWindow(const Uid& id)
    {
        auto window = GetWindow(id);
        window->Close();
    }

    void WindowManager::CloseAllWindows()
    {
        for (auto window : _stack)
        {
            window->Close();
        }
    }
}