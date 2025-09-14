#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Layers/LogLayer.h"
#include "Tbx/Layers/InputLayer.h"
#include "Tbx/Layers/WorldLayer.h"
#include "Tbx/Layers/RenderingLayer.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Input/InputCodes.h"
#include "Tbx/Time/DeltaTime.h"

namespace Tbx
{
    App::App(const std::string_view& name)
    {
        _name = name;
        _status = AppStatus::Initializing;
        Launch();
    }

    App::~App()
    {
        if (_status != AppStatus::Closed || _status == AppStatus::Closing) 
        {
            Close();
        }
    }
    
    void App::Launch()
    {
        try
        {
            _status = AppStatus::Initializing;

            // Init required systems
            _eventBus = std::make_shared<EventBus>();
            _pluginManager = std::make_shared<PluginManager>(FileSystem::GetWorkingDirectory(), _eventBus);
            // TODO: windowing should be a layer, all app core systems should be a layer!
            _windowManager = std::make_shared<WindowManager>(_pluginManager->GetPlugin<IWindowFactory>(), _eventBus);
            _layerManager = std::make_shared<LayerManager>();

            // Init required layers
            {
                const auto world = std::make_shared<WorldLayer>();
                const auto rendering = std::make_shared<RenderingLayer>(world->GetWorldSpace(), _pluginManager->GetPlugin<IRendererFactory>(), _eventBus);
                const auto input = std::make_shared<InputLayer>(_pluginManager->GetPlugin<IInputHandler>());
                const auto log = std::make_shared<LogLayer>(_pluginManager->GetPlugin<ILoggerFactory>());
                _layerManager->AddLayer(log);
                _layerManager->AddLayer(input);
                _layerManager->AddLayer(rendering);
                _layerManager->AddLayer(world);
            }

            std::filesystem::path cwd = std::filesystem::current_path();
            TBX_TRACE_INFO("Current working directory is: {}", cwd.string());

            _eventBus->Subscribe(this, &App::OnWindowClosed);
            _windowManager->OpenWindow(_name, WindowMode::Windowed, Size(1920, 1080));

            OnLaunch();

            _status = AppStatus::Running;
            while (_status == AppStatus::Running)
            {
                Update();
            }
        }
        catch (std::exception ex)
        {
            _status = AppStatus::Error;
            TBX_ASSERT(false, ex.what());
        }

        if (_status != AppStatus::Reloading)
        {
            _status = AppStatus::Closed;
        }
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
            TBX_TRACE("Closing app...");
            _status = AppStatus::Closing;
            return;
        }

        // Shortcut to restart/reload app
        if (Input::IsKeyDown(TBX_KEY_F2))
        {
            TBX_TRACE("Reloading app...");
            _status = AppStatus::Reloading;
            return;
        }
#endif

        OnUpdate();

        _layerManager->UpdateLayers();
        _windowManager->UpdateWindows();

        if (_status != AppStatus::Running) return;

        _eventBus->Post(AppUpdatedEvent());
        _eventBus->ProcessQueue();
    }

    void App::Close()
    {
        TBX_TRACE_INFO("Shutting down...");

        auto isRestarting = _status == AppStatus::Reloading;
        _status = AppStatus::Closing;

        OnShutdown();

        AppExitingEvent exitEvent;
        _eventBus->Send(exitEvent);
        _eventBus->Unsubscribe(this, &App::OnWindowClosed);

        _status = isRestarting
            ? AppStatus::Reloading
            : AppStatus::Closed;
    }

    void App::OnWindowClosed(const WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        auto window = e.GetWindow();
        if (window && window->GetId() == _windowManager->GetMainWindow()->GetId())
        {
            // Stop running and close all windows
            _status = AppStatus::Closing;
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

    std::shared_ptr<EventBus> App::GetEventBus()
    {
        return _eventBus;
    }

    std::shared_ptr<LayerManager> App::GetLayerManager()
    {
        return _layerManager;
    }

    std::shared_ptr<WindowManager> App::GetWindowManager()
    {
        return _windowManager;
    }

    std::shared_ptr<PluginManager> App::GetPluginManager()
    {
        return _pluginManager;
    }

    void App::SetSettings(const Settings& settings)
    {
        _settings = settings;
        _eventBus->Post(AppSettingsChangedEvent(settings));
    }

    const Settings& App::GetSettings() const
    {
        return _settings;
    }
}
