#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Layers/LogLayer.h"
#include "Tbx/Layers/InputLayer.h"
#include "Tbx/Layers/WorldLayer.h"
#include "Tbx/Layers/EventCoordinatorLayer.h"
#include "Tbx/Events/EventCoordinator.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Time/DeltaTime.h"
#include "Tbx/TBS/World.h"

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
            Close();
        }
    }

    void App::OnLoad()
    {
        // Do nothing by default
    }

    void App::OnUnload()
    {
        // Do nothing by default
    }
    
    void App::Launch()
    {
        // Where are we?
        std::filesystem::path cwd = std::filesystem::current_path();
        TBX_TRACE_INFO("Current working directory is: {}", cwd.string());

        auto world = std::make_shared<World>();
        _status = AppStatus::Initializing;

        EventCoordinator::Subscribe(this, &App::OnWindowClosed);

        // Add default layers (order is important as they will be updated and destroyed in reverse order)
        EmplaceLayer<EventCoordinatorLayer>();
        EmplaceLayer<WorldLayer>(world);
        EmplaceLayer<InputLayer>();

        // Open a main window
        OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));

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

        // Shortcut to restart/reload app
        if (Input::IsKeyDown(TBX_KEY_F2))
        {
            _status = AppStatus::Reloading;
            return;
        }
#endif

        // Call on update for app inheritors
        OnUpdate();

        // Update windows
        for (const auto& window : _windowStack)
        {
            window->Update();
            if (_status != AppStatus::Running) return;
        }

        // Update layers
        for (const auto& layer : _layerStack)
        {
            layer->Update();
            if (_status != AppStatus::Running) return;
        }

        if (_status != AppStatus::Running) return;

        // Send update event
        AppUpdatedEvent updateEvent;
        EventCoordinator::Send(updateEvent);
    }

    void App::Close()
    {
        TBX_TRACE_INFO("Shutting down...");

        auto isRestarting = _status == AppStatus::Reloading;
        _status = AppStatus::Exiting;

        OnShutdown();

        // Unsub to window events and shutdown events
        EventCoordinator::Unsubscribe(this, &App::OnWindowClosed);

        // Clear windows
        _windowStack.Clear();

        // Clear layers
        _layerStack.Clear();

        // Set status to closed or reloading if we are reloading
        _status = isRestarting
            ? AppStatus::Reloading
            : AppStatus::Closed;
    }

    const std::vector<std::shared_ptr<IWindow>>& App::GetWindows()
    {
        return _windowStack.GetAll();
    }

    std::shared_ptr<IWindow> App::GetWindow(Uid id)
    {
        return _windowStack.Get(id);
    }

    Uid App::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        auto newWindowId = _windowStack.Emplace(name, size, mode);
        const auto& openWindows = _windowStack.GetAll();
        if (openWindows.size() == 1)
        {
            _mainWindowId = newWindowId;
        }
        return newWindowId;
    }

    void App::OnWindowClosed(const WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        auto window = e.GetWindow();
        if (window && window->GetId() == _mainWindowId)
        {
            // Stop running and close all windows
            _status = AppStatus::Exiting;
        }

        if (window)
        {
            _windowStack.Remove(window->GetId());
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
        auto mainWindow = _windowStack.Get(_mainWindowId);
        return mainWindow;
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
