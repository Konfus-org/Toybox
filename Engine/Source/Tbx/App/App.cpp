#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Layers/LogLayer.h"
#include "Tbx/Layers/InputLayer.h"
#include "Tbx/Layers/EventLayer.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Time/DeltaTime.h"

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
        std::filesystem::path cwd = std::filesystem::current_path();
        TBX_TRACE_INFO("Current working directory is: {}", cwd.string());

        _status = AppStatus::Initializing;

        EventCoordinator::Subscribe(this, &App::OnWindowOpened);
        EventCoordinator::Subscribe(this, &App::OnWindowClosed);

        AddLayer(std::make_shared<LogLayer>(shared_from_this()));
        AddLayer(std::make_shared<InputLayer>(shared_from_this()));
        AddLayer(std::make_shared<EventLayer>(shared_from_this()));
        AddLayer(std::make_shared<WorldLayer>(shared_from_this()));
        AddLayer(std::make_shared<RenderingLayer>(shared_from_this()));

        OpenWindow(_name, WindowMode::Windowed, Size(1920, 1080));

        OnLaunch();

        _status = AppStatus::Running;
    }

    void App::Update()
    {
        if (_status != AppStatus::Running) return;

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

        OnUpdate();
        UpdateLayers();
        UpdateWindows();

        if (_status != AppStatus::Running) return;

        AppUpdatedEvent updateEvent;
        EventCoordinator::Send(updateEvent);
    }

    void App::Close()
    {
        TBX_TRACE_INFO("Shutting down...");

        auto isRestarting = _status == AppStatus::Reloading;
        _status = AppStatus::Exiting;

        OnShutdown();

        EventCoordinator::Unsubscribe(this, &App::OnWindowClosed);

        CloseAllWindows();
        ClearLayers();

        _status = isRestarting
            ? AppStatus::Reloading
            : AppStatus::Closed;
    }

    void App::OnWindowOpened(const WindowOpenedEvent& e)
    {
        auto newWindow = e.GetWindow();
        if (_mainWindowId == Invalid::Uid)
        {
            _mainWindowId = newWindow->GetId();
        }
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
            CloseWindow(window->GetId());
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

    std::shared_ptr<IWindow> App::GetMainWindow() const
    {
        auto mainWindow = GetWindow(_mainWindowId);
        return mainWindow;
    }

    std::shared_ptr<WorldLayer> App::GetWorld() const
    {
        return GetLayer<WorldLayer>();
    }

    std::shared_ptr<RenderingLayer> App::GetRendering() const
    {
        return GetLayer<RenderingLayer>();
    }

    void App::SetSettings(const Settings& settings)
    {
        _settings = settings;
        auto graphicsSettingsChangedEvent = AppSettingsChangedEvent(settings);
        EventCoordinator::Send(graphicsSettingsChangedEvent);
    }

    const Settings& App::GetSettings() const
    {
        return _settings;
    }
}
