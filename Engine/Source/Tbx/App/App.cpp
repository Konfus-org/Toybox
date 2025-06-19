#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Layers/InputLayer.h"
#include "Tbx/Layers/WorldLayer.h"
#include "Tbx/Layers/WindowingLayer.h"
#include "Tbx/Layers/EventCoordinatorLayer.h"
#include "Tbx/Layers/RenderingLayer.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/ApplicationEvents.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Time/DeltaTime.h"

namespace Tbx
{
    std::shared_ptr<App> App::_instance = nullptr;

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

    std::shared_ptr<App> App::GetInstance()
    {
        return _instance;
    }
    
    void App::Launch()
    {
        TBX_ASSERT(_instance == nullptr, "There is an existing instance still running!");

        _instance = std::shared_ptr<App>(this);
        _status = AppStatus::Initializing;
        _windowClosedEventId = EventCoordinator::Subscribe<WindowClosedEvent>(TBX_BIND_FN(OnWindowClosed));

        // Add default layers (order is important as they will be updated and destroyed in reverse order)
        PushLayer(std::make_shared<EventCoordinatorLayer>());
        PushLayer(std::make_shared<WindowingLayer>());
        PushLayer(std::make_shared<RenderingLayer>());
        PushLayer(std::make_shared<WorldLayer>());
        PushLayer(std::make_shared<InputLayer>());

        WindowManager::OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));

        OnLaunch();

        _status = AppStatus::Running;
    }

    void App::Update()
    {
        if (_status != AppStatus::Running) return;

        // Update delta time
        Time::DeltaTime::DrawFrame();

#ifndef TBX_RELEASE
        // Only allow reloading and force quit when not released!

        // Shortcut to kill the app
        if (Input::IsKeyDown(TBX_KEY_F4) &&
            (Input::IsKeyDown(TBX_KEY_LEFT_ALT) || Input::IsKeyDown(TBX_KEY_RIGHT_ALT)))
        {
            _status = AppStatus::Exiting;
            return;
        }

        // Shortcut to restart app
        if (Input::IsKeyDown(TBX_KEY_F2))
        {
            _status = AppStatus::Restarting;
            return;
        }

        // Shortcut to hot reload plugins
        if (Input::IsKeyDown(TBX_KEY_F3))
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
        // Shutdown and clear resources
        OnShutdown();
        ShutdownSystems();

        // Clear instance
        _instance = nullptr;
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
        _status = oldStatus == AppStatus::Reloading || oldStatus == AppStatus::Restarting
            ? oldStatus
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
        if (e.GetWindowId() == WindowManager::GetMainWindow()->GetId())
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

    const GraphicsSettings& App::GetGraphicsSettings() const
    {
        return _graphicsSettings;
    }
}
