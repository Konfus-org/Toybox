#include "Tbx/Runtime/PCH.h"
#include "Tbx/Runtime/App/App.h"
#include "Tbx/Runtime/Input/Input.h"
#include "Tbx/Runtime/Time/DeltaTime.h"
#include "Tbx/Runtime/Render Pipeline/RenderPipeline.h"
#include "Tbx/Runtime/Windowing/WindowManager.h"
#include "Tbx/Runtime/World/WorldLayer.h"
#include "Tbx/Runtime/Events/EventCoordinatorLayer.h"
#include "Tbx/Runtime/Events/ApplicationEvents.h"
#include <Tbx/Core/Events/EventCoordinator.h>

namespace Tbx
{
    App::App(const std::string_view& name)
    {
        _name = name;
    }

    App::~App()
    {
        if (_status == AppStatus::Running) 
        {
            ShutdownSystems();
        }
    }
    
    void App::Launch()
    {
        _status = AppStatus::Initializing;

        // Subscribe to window events
        _windowClosedEventId = EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_FN(OnWindowClosed));

        // Add default layers (order is important as they will be updated and destroyed in reverse order)
        PushLayer(std::make_shared<Input>("Input"));
        PushLayer(std::make_shared<WorldLayer>("World"));
        PushLayer(std::make_shared<RenderPipeline>("Rendering"));
        PushLayer(std::make_shared<WindowManager>("Windowing"));
        PushLayer(std::make_shared<EventCoordinatorLayer>("Events"));

        // Open main window
        WindowManager::OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));
        auto mainWindow = WindowManager::GetMainWindow();
        auto windowFocusChangedEvent = WindowFocusChangedEvent(mainWindow.lock()->GetId(), true);
        EventCoordinator::Send(windowFocusChangedEvent);

        // Set default graphics settings
        auto graphicsSettingsChangedEvent = AppGraphicsSettingsChangedEvent(_graphicsSettings);
        EventCoordinator::Send(graphicsSettingsChangedEvent);

        OnLaunch();

        _status = AppStatus::Running;
    }

    void App::Update()
    {
        if (_status != AppStatus::Running) return;

        // Update delta time
        Time::DeltaTime::Update();

#ifndef TBX_RELEASE
        // Only allow reloading and force quit when not released!

        // Shortcut to kill the app
        if (Input::IsKeyDown(TBX_KEY_F4) &&
            (Input::IsKeyDown(TBX_KEY_LEFT_ALT) || Input::IsKeyDown(TBX_KEY_RIGHT_ALT)))
        {
            _status = AppStatus::Exiting;
            return;
        }

        // Shortcut to hot reload plugins
        if (Input::IsKeyDown(TBX_KEY_F5))
        {
            _status = AppStatus::Reloading;
            return;
        }
#endif

        // Call on update for app inheritors
        OnUpdate();

        // Update layers
        for (const auto& layer : _layerStack)
        {
            if (_status != AppStatus::Running) return;

            layer->OnUpdate();
        }

        if (_status != AppStatus::Running) return;

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

        auto oldStatus = _status;
        _status = AppStatus::Exiting;

        // Unsub to window events and shutdown events
        EventCoordinator::Unsubscribe<WindowClosedEvent>(_windowClosedEventId);

        // Clear layers
        _layerStack.Clear();

        // Set status to closed or reloading if we are reloading
        _status = oldStatus == AppStatus::Reloading 
            ? AppStatus::Reloading 
            : AppStatus::Closed;
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
            _status = AppStatus::Exiting;
        }
    }

    const AppStatus& App::GetStatus() const
    {
        return _status;
    }

    const std::string& App::GetName() const
    {
        return _name;
    }

    std::weak_ptr<IWindow> App::GetMainWindow() const
    {
        return WindowManager::GetMainWindow();
    }

    void App::SetGraphicsSettings(const GraphicsSettings& settings)
    {
        _graphicsSettings = settings;
        auto graphicsSettingsChangedEvent = AppGraphicsSettingsChangedEvent(settings);
        EventCoordinator::Send(graphicsSettingsChangedEvent);
    }
}