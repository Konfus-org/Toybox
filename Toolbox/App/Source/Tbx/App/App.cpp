#include "Tbx/App/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Input/Input.h"
#include "Tbx/App/Time/DeltaTime.h"
#include "Tbx/App/Render Pipeline/RenderPipeline.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include "Tbx/App/Events/ApplicationEvents.h"
#include <Tbx/Core/Events/EventDispatcher.h>

namespace Tbx
{
    std::shared_ptr<App> App::_instance = nullptr;

    App::App(const std::string_view& name)
    {
        _name = name;
        _isRunning = false;
    }

    App::~App()
    {
        if (_isRunning) 
        {
            ShutdownSystems();
        }
    }
    
    void App::Launch(bool headless)
    {
        _instance = std::shared_ptr<App>(this);
        _isRunning = true;
        _isHeadless = headless;

        if (!_isHeadless)
        {
            // Subscribe to window events
            _windowClosedEventId = EventDispatcher::Subscribe<WindowClosedEvent>(TBX_BIND_CALLBACK(OnWindowClosed));

            // Init rendering
            RenderPipeline::Initialize();

            // Init input
            Input::Initialize();

            // Open main window
            WindowManager::Initialize();
            WindowManager::OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));
            auto mainWindow = WindowManager::GetMainWindow();
        }

        OnLaunch();
    }

    void App::Update()
    {
        // Update delta time
        Time::DeltaTime::Update();

        // Call on update for app inheritors
        OnUpdate();

        // Send update event
        AppUpdatedEvent updateEvent;
        EventDispatcher::Send(updateEvent);

        // Then update layers
        for (const auto& layer : _layerStack)
        {
            layer->OnUpdate();
        }
    }

    void App::Close()
    {
        OnShutdown();
        ShutdownSystems();
    }

    void App::ShutdownSystems()
    {
        _isRunning = false;

        // Unsub to window events and shutdown events
        EventDispatcher::Unsubscribe(_windowClosedEventId);

        // Call detach on all layers
        for (const auto& layer : _layerStack)
        {
            layer->OnDetach();
        }

        if (!_isHeadless)
        {
            // Close all windows, shutdown rendering and input if we are not headless.
            WindowManager::Shutdown();
            RenderPipeline::Shutdown();
            Input::Shutdown();
        }

        // Finally close the log and shutdown events, this should be the last thing to happen before modules are unloaded
        Log::Close();
        EventDispatcher::Clear();
    }

    void App::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size) const
    {
        WindowManager::OpenNewWindow(name, mode, size);
    }

    void App::PushLayer(const std::shared_ptr<Layer>& layer)
    {
        if (layer->IsOverlay())
        {
            _layerStack.PushOverlay(layer);
        }
        else 
        {
            _layerStack.PushLayer(layer);
        }
        layer->OnAttach();
    }

    void App::OnWindowClosed(const WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        if (e.GetWindowId() == WindowManager::GetMainWindow().lock()->GetId())
        {
            // Stop running and close all windows
            _isRunning = false;
            WindowManager::CloseAllWindows();
        }
    }

    bool App::IsRunning() const
    {
        return _isRunning;
    }

    const std::string& App::GetName() const
    {
        return _name;
    }

    std::weak_ptr<IWindow> App::GetMainWindow() const
    {
        return WindowManager::GetMainWindow();
    }

    std::weak_ptr<App> App::GetInstance()
    {
        return _instance;
    }
}
