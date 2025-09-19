#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Layers/LogLayer.h"
#include "Tbx/Layers/InputLayer.h"
#include "Tbx/Layers/RenderingLayer.h"
#include "Tbx/Layers/RuntimeLayer.h"
#include "Tbx/Layers/WindowingLayer.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Input/InputCodes.h"
#include "Tbx/Time/DeltaTime.h"
#include "Tbx/Files/Paths.h"

namespace Tbx
{
    App::App(const std::string_view& name)
    {
        _name = name;
    }

    App::~App()
    {
        if (_status != AppStatus::Closed)
        {
            Shutdown();
        }
    }

    void App::Run()
    {
        try
        {
            Initialize();

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

    void App::Close()
    {
        Shutdown();
    }
    
    void App::Initialize()
    {
        _status = AppStatus::Initializing;

        auto workingDirectory = FileSystem::GetWorkingDirectory();
        //std::filesystem::path cwd = std::filesystem::current_path();
        TBX_TRACE_INFO("Current working directory is: {}", workingDirectory);

        // Init required systems
        auto self = shared_from_this();
        _eventBus = std::make_shared<EventBus>();
        _pluginServer = std::make_shared<PluginServer>(workingDirectory, _eventBus, self);
        _assetServer = std::make_shared<AssetServer>(workingDirectory, _pluginServer->GetPlugins<IAssetLoader>());
        _layerManager = std::make_shared<LayerManager>();

        // Init required layers
        auto rendering = std::make_shared<RenderingLayer>(_pluginServer->GetPlugin<IRendererFactory>(), _eventBus);
        auto input = std::make_shared<InputLayer>(_pluginServer->GetPlugin<IInputHandler>());
        auto log = std::make_shared<LogLayer>(_pluginServer->GetPlugin<ILoggerFactory>());
        auto windowingLayer = std::make_shared<WindowingLayer>(_name, _pluginServer->GetPlugin<IWindowFactory>(), _eventBus);
        auto runtime = std::make_shared<RuntimeLayer>(weak_from_this());
        _layerManager->AddLayer(log);
        _layerManager->AddLayer(input);
        _layerManager->AddLayer(windowingLayer);
        _layerManager->AddLayer(runtime);
        _layerManager->AddLayer(rendering);

        _eventBus->Subscribe(this, &App::OnWindowClosed);

        OnLaunch();
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

        if (_status != AppStatus::Running) return;

        _eventBus->Post(AppUpdatedEvent());
        _eventBus->ProcessQueue();
    }

    void App::Shutdown()
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
        const auto windowManager = GetWindowManager();
        if (!windowManager)
        {
            return;
        }

        auto mainWindow = windowManager->GetMainWindow();
        if (window && mainWindow && window->GetId() == mainWindow->GetId())
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
        if (!_layerManager)
        {
            return nullptr;
        }

        const auto windowingLayer = _layerManager->GetLayer<WindowingLayer>();
        return windowingLayer ? windowingLayer->GetWindowManager() : nullptr;
    }

    std::shared_ptr<PluginServer> App::GetPluginServer()
    {
        return _pluginServer;
    }

    std::shared_ptr<AssetServer> App::GetAssetServer()
    {
        return _assetServer;
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

    void App::AddRuntime(const std::shared_ptr<IRuntime>& runtime)
    {
        if (!runtime)
        {
            return;
        }

        const auto runtimeLayer = _layerManager ? _layerManager->GetLayer<RuntimeLayer>() : nullptr;
        TBX_ASSERT(runtimeLayer, "Runtime layer must exist before adding runtimes");
        runtimeLayer->AddRuntime(runtime);
    }

    void App::RemoveRuntime(const std::shared_ptr<IRuntime>& runtime)
    {
        if (!runtime)
        {
            return;
        }

        const auto runtimeLayer = _layerManager ? _layerManager->GetLayer<RuntimeLayer>() : nullptr;
        if (runtimeLayer)
        {
            runtimeLayer->RemoveRuntime(runtime);
        }
    }

    std::vector<std::shared_ptr<IRuntime>> App::GetRuntimes() const
    {
        const auto runtimeLayer = _layerManager ? _layerManager->GetLayer<RuntimeLayer>() : nullptr;
        if (runtimeLayer)
        {
            return runtimeLayer->GetRuntimes();
        }

        return {};
    }
}
