#include "Tbx/PCH.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Debug/Debugging.h"

namespace Tbx
{
    WindowManager::WindowManager(
        std::shared_ptr<IWindowFactory> windowFactory, 
        std::shared_ptr<EventBus> eventBus)
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

    void WindowManager::UpdateWindows()
    {
        for (auto& window : _stack)
        {
            window->Update();
            if (window->IsClosed())
            {
                _stack.Remove(window->GetId());
                _eventBus->Post(WindowClosedEvent(window));
            }
            if (window->IsFocused())
            {
                _eventBus->Post(WindowFocusedEvent(window));
            }
        }
    }

    const std::vector<std::shared_ptr<IWindow>>& WindowManager::GetAllWindows() const
    {
        return _stack.GetAll();
    }

    std::shared_ptr<IWindow> WindowManager::GetMainWindow() const
    {
        return GetWindow(_mainWindowId);
    }

    std::shared_ptr<IWindow> WindowManager::GetWindow(const Uid& id) const
    {
        return _stack.Get(id);
    }

    Uid WindowManager::OpenWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        std::shared_ptr<IWindow> window = _windowFactory->Create(name, size, mode);
        TBX_ASSERT(window, "Failed to create window!");
        if (_mainWindowId == Uid::Invalid)
        {
            _mainWindowId = window->GetId();
        }
        
        _stack.Push(window);
        window->Open();
        window->Focus();
        _eventBus->Post(WindowOpenedEvent(window));

        return window->GetId();
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