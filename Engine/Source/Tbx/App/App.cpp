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
        catch (const std::exception& ex)
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
        auto runtime = std::make_shared<RuntimeLayer>(self);
        AddLayer(log);
        AddLayer(input);
        AddLayer(windowingLayer);
        AddLayer(runtime);
        AddLayer(rendering);

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

        TBX_ASSERT(_layerManager, "Layer manager must exist before updating layers.");
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

    void App::SetSettings(const Settings& settings)
    {
        _settings = settings;
        _eventBus->Post(AppSettingsChangedEvent(settings));
    }

    const Settings& App::GetSettings() const
    {
        return _settings;
    }

    const AppStatus& App::GetStatus() const
    {
        return _status;
    }

    const std::string& App::GetName() const
    {
        return _name;
    }

    Ref<EventBus> App::GetEventBus() const
    {
        return _eventBus;
    }

    Ref<PluginServer> App::GetPluginServer() const
    {
        return _pluginServer;
    }

    Ref<AssetServer> App::GetAssetServer() const
    {
        return _assetServer;
    }

    void App::AddLayer(Ref<Layer> layer) const
    {
        _layerManager->AddLayer(layer);
    }

    void App::RemoveLayer(const std::string& name) const
    {
        _layerManager->RemoveLayer(name);
    }

    void App::RemoveLayer(Ref<Layer> layer) const
    {
        _layerManager->RemoveLayer(layer);
    }

    Ref<Layer> App::GetLayer(const std::string& name) const
    {
        return _layerManager->GetLayer(name);
    }

    std::vector<Ref<Layer>> App::GetLayers() const
    {
        return _layerManager->GetLayers();
    }

    Uid App::OpenWindow(const std::string& name, const WindowMode& mode, const Size& size) const
    {
        auto windowManager = GetWindowManager();
        return windowManager->OpenWindow(name, mode, size);
    }

    void App::CloseWindow(const Uid& id) const
    {
        auto windowManager = GetWindowManager();
        windowManager->CloseWindow(id);
    }

    void App::CloseAllWindows() const
    {
        auto windowManager = GetWindowManager();
        windowManager->CloseAllWindows();
    }

    std::vector<Ref<IWindow>> App::GetOpenWindows() const
    {
        auto windowManager = GetWindowManager();
        const auto& windows = windowManager->GetAllWindows();
        return { windows.begin(), windows.end() };
    }

    Ref<IWindow> App::GetWindow(const Uid& id) const
    {
        auto windowManager = GetWindowManager();
        return windowManager->GetWindow(id);
    }

    Ref<IWindow> App::GetMainWindow() const
    {
        auto windowManager = GetWindowManager();
        return windowManager->GetMainWindow();
    }

    Ref<WindowManager> App::GetWindowManager() const
    {
        auto windowingLayer = _layerManager->GetLayer<WindowingLayer>();
        auto windowManager = windowingLayer->GetWindowManager();
        return windowManager;
    }

    void App::AddRuntime(Ref<IRuntime> runtime) const
    {
        auto runtimeLayer = _layerManager->GetLayer<RuntimeLayer>();
        runtimeLayer->AddRuntime(runtime);
    }

    void App::RemoveRuntime(Ref<IRuntime> runtime) const
    {
        const auto runtimeLayer = _layerManager->GetLayer<RuntimeLayer>();
        runtimeLayer->RemoveRuntime(runtime);
    }

    std::vector<Ref<IRuntime>> App::GetRuntimes() const
    {
        const auto runtimeLayer = _layerManager->GetLayer<RuntimeLayer>();
        return runtimeLayer->GetRuntimes();
    }

    void App::OnWindowClosed(const WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        const auto window = e.GetWindow();
        const auto mainWindow = GetMainWindow();
        if (window && mainWindow && window->GetId() == mainWindow->GetId())
        {
            // Stop running and close all windows
            _status = AppStatus::Closing;
        }
    }
}
