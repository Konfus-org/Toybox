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

        // Init required layers
        auto rendering = std::make_shared<RenderingLayer>(_pluginServer->GetPlugin<IRendererFactory>(), _eventBus);
        auto input = std::make_shared<InputLayer>(_pluginServer->GetPlugin<IInputHandler>());
        auto log = std::make_shared<LogLayer>(_pluginServer->GetPlugin<ILoggerFactory>());
        auto windowingLayer = std::make_shared<WindowingLayer>(_name, _pluginServer->GetPlugin<IWindowFactory>(), _eventBus);
        auto runtime = std::make_shared<RuntimeLayer>(self);
        const bool logLayerAdded = AddLayer(log);
        TBX_ASSERT(logLayerAdded, "Failed to add the log layer. A layer with the same name already exists.");

        const bool inputLayerAdded = AddLayer(input);
        TBX_ASSERT(inputLayerAdded, "Failed to add the input layer. A layer with the same name already exists.");

        const bool windowingLayerAdded = AddLayer(windowingLayer);
        TBX_ASSERT(windowingLayerAdded, "Failed to add the windowing layer. A layer with the same name already exists.");

        const bool runtimeLayerAdded = AddLayer(runtime);
        TBX_ASSERT(runtimeLayerAdded, "Failed to add the runtime layer. A layer with the same name already exists.");

        const bool renderingLayerAdded = AddLayer(rendering);
        TBX_ASSERT(renderingLayerAdded, "Failed to add the rendering layer. A layer with the same name already exists.");

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

        _layers.Update();

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
        const auto window = e.GetWindow();
        const auto mainWindow = GetMainWindow();
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

    Tbx::Ref<EventBus> App::GetEventBus()
    {
        return _eventBus;
    }

    bool App::AddLayer(const Tbx::Ref<Layer>& layer)
    {
        if (!layer)
        {
            return false;
        }

        const auto existingLayer = _layers.GetLayer(layer->GetName());
        if (existingLayer)
        {
            TBX_ASSERT(false, "Layer names must be unique. A layer named {} is already registered.", layer->GetName());
            return false;
        }

        _layers.Push(layer);
        return true;
    }

    bool App::RemoveLayer(const std::string& name)
    {
        auto layer = _layers.GetLayer(name);
        if (!layer)
        {
            return false;
        }

        _layers.Remove(name);
        return true;
    }

    bool App::RemoveLayer(const Tbx::Ref<Layer>& layer)
    {
        if (!layer)
        {
            return false;
        }

        auto existingLayer = _layers.GetLayer(layer->GetName());
        if (!existingLayer)
        {
            return false;
        }

        _layers.Remove(layer);
        return true;
    }

    Tbx::Ref<Layer> App::GetLayer(const std::string& name) const
    {
        return _layers.GetLayer(name);
    }

    std::vector<Tbx::Ref<Layer>> App::GetLayers() const
    {
        return _layers.GetLayers();
    }

    Tbx::Ref<PluginServer> App::GetPluginServer()
    {
        return _pluginServer;
    }

    Tbx::Ref<AssetServer> App::GetAssetServer()
    {
        return _assetServer;
    }

    void App::SetSettings(const Settings& settings)
    {
        _settings = settings;
        if (_eventBus)
        {
            _eventBus->Post(AppSettingsChangedEvent(settings));
        }
    }

    const Settings& App::GetSettings() const
    {
        return _settings;
    }

    void App::AddRuntime(const Tbx::Ref<IRuntime>& runtime)
    {
        if (!runtime)
        {
            return;
        }

        const auto runtimeLayer = _layers.GetLayer<RuntimeLayer>();
        TBX_ASSERT(runtimeLayer, "Runtime layer must exist before adding runtimes");
        runtimeLayer->AddRuntime(runtime);
    }

    void App::RemoveRuntime(const Tbx::Ref<IRuntime>& runtime)
    {
        if (!runtime)
        {
            return;
        }

        const auto runtimeLayer = _layers.GetLayer<RuntimeLayer>();
        if (runtimeLayer)
        {
            runtimeLayer->RemoveRuntime(runtime);
        }
    }

    std::vector<Tbx::Ref<IRuntime>> App::GetRuntimes() const
    {
        const auto runtimeLayer = _layers.GetLayer<RuntimeLayer>();
        if (runtimeLayer)
        {
            return runtimeLayer->GetRuntimes();
        }

        return {};
    }

    Uid App::OpenWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        const auto windowManager = GetWindowManager();
        return windowManager ? windowManager->OpenWindow(name, mode, size) : Uid::Invalid;
    }

    void App::CloseWindow(const Uid& id)
    {
        const auto windowManager = GetWindowManager();
        if (windowManager)
        {
            windowManager->CloseWindow(id);
        }
    }

    void App::CloseAllWindows()
    {
        const auto windowManager = GetWindowManager();
        if (windowManager)
        {
            windowManager->CloseAllWindows();
        }
    }

    std::vector<Tbx::Ref<IWindow>> App::GetOpenWindows() const
    {
        const auto windowManager = GetWindowManager();
        if (!windowManager)
        {
            return {};
        }

        const auto& windows = windowManager->GetAllWindows();
        return { windows.begin(), windows.end() };
    }

    Tbx::Ref<IWindow> App::GetWindow(const Uid& id) const
    {
        const auto windowManager = GetWindowManager();
        return windowManager ? windowManager->GetWindow(id) : nullptr;
    }

    Tbx::Ref<IWindow> App::GetMainWindow() const
    {
        const auto windowManager = GetWindowManager();
        return windowManager ? windowManager->GetMainWindow() : nullptr;
    }

    Tbx::Ref<WindowManager> App::GetWindowManager() const
    {
        const auto windowingLayer = _layers.GetLayer<WindowingLayer>();
        return windowingLayer ? windowingLayer->GetWindowManager() : nullptr;
    }
}
