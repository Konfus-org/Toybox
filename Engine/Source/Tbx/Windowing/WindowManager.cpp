#include "Tbx/PCH.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Input/IInputHandler.h"
#include "Tbx/Input/InputCodes.h"
#include "Tbx/Debug/Asserts.h"

namespace Tbx
{
    WindowManager::WindowManager() = default;

    WindowManager::WindowManager(
        Ref<IWindowFactory> windowFactory,
        Ref<IInputHandler> inputHandler,
        Ref<EventBus> eventBus)
    {
        _windowFactory = windowFactory;
        _inputHandler = inputHandler;
        _eventBus = eventBus;
        TBX_ASSERT(_eventBus, "Window Manager: given invalid event bus!");
    }

    WindowManager::~WindowManager()
    {
        CloseAllWindows();
    }

    void WindowManager::Update() const
    {
        bool toggleFullscreen = false;
        if (_inputHandler && _inputHandler->IsKeyDown(TBX_KEY_F11))
        {
            toggleFullscreen = true;
        }

        for (const auto& window : _stack)
        {
            if (toggleFullscreen)
            {
                const auto windowIsFullscreen = window->GetMode() == WindowMode::Fullscreen;
                if (windowIsFullscreen)
                {
                    window->SetMode(WindowMode::Windowed);
                }
                else
                {
                    window->SetMode(WindowMode::Fullscreen);
                }
            }
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
        if (_windowFactory == nullptr)
        {
            TBX_ASSERT(false, "Window Manager:  factory not set!");
            return Uid::Invalid;
        }

        Ref<Window> window = _windowFactory->Create(name, size, mode, _eventBus);
        TBX_ASSERT(window, "Window Manager: failed to create window!");
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