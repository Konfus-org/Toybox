#include "Tbx/App/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Input/Input.h"
#include "Tbx/App/Time/DeltaTime.h"
#include "Tbx/App/Render Pipeline/RenderPipeline.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include "Tbx/App/Events/ApplicationEvents.h"
#include <Tbx/Core/Events/EventCoordinator.h>

namespace Tbx
{
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
        _isRunning = true;
        _isHeadless = headless;

        if (!_isHeadless)
        {
            // Subscribe to window events
            _windowClosedEventId = EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_FN(OnWindowClosed));

            // Init rendering
            RenderPipeline::Initialize();

            // Init input
            Input::Initialize();

            // Open main window
            WindowManager::Initialize();
            WindowManager::OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));
            auto mainWindow = WindowManager::GetMainWindow();

            // Tell things the main window should be focused on
            auto windowFocusChangedEvent = WindowFocusChangedEvent(mainWindow.lock()->GetId(), true);
            EventCoordinator::Send(windowFocusChangedEvent);
        }

        OnLaunch();
    }

    void App::Update()
    {
        if (!_isRunning) return;

        // Update delta time
        Time::DeltaTime::Update();

        // Call on update for app inheritors
        OnUpdate();

        // Update layers
        for (const auto& layer : _layerStack)
        {
            layer->OnUpdate();
        }

        // Send update event
        AppUpdatedEvent updateEvent;
        EventCoordinator::Send(updateEvent);
    }

    void App::Close()
    {
        OnShutdown();
        ShutdownSystems();
    }

    void App::ShutdownSystems()
    {
        TBX_INFO("Shutting down...");

        _isRunning = false;

        // Unsub to window events and shutdown events
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_windowClosedEventId);

        // Clear layers
        _layerStack.Clear();

        if (!_isHeadless)
        {
            // Close all windows, shutdown rendering and input if we are not headless.
            WindowManager::Shutdown();
            RenderPipeline::Shutdown();
            Input::Shutdown();
        }
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
}