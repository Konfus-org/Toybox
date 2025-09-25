#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/Layers/LogLayer.h"
#include "Tbx/Layers/InputLayer.h"
#include "Tbx/Layers/RenderingLayer.h"
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
        // If we haven't closed yet
        if (_status != AppStatus::Closed &&
            _status != AppStatus::Error)
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

            Shutdown();
        }
        catch (const std::exception& ex)
        {
            _status = AppStatus::Error;
            TBX_ASSERT(false, ex.what());
        }

        if (_status != AppStatus::Reloading &&
            _status != AppStatus::Error)
        {
            _status = AppStatus::Closed;
        }
    }

    void App::Close()
    {
        _status = AppStatus::Closing;
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

        // Load plugin driven systems
        {
            auto windowFactoryPlugs = _pluginServer->GetPlugins<IWindowFactory>();
            if (!windowFactoryPlugs.empty())
            {
                TBX_ASSERT(windowFactoryPlugs.size() == 1, "Only one window factory is allowed!");
                auto windowFactory = windowFactoryPlugs.front();
                _windowManager = std::make_shared<WindowManager>(windowFactory, _eventBus);
                AddLayer<WindowingLayer>(_name, _windowManager);
            }

            auto rendererFactoryPlugs = _pluginServer->GetPlugins<IRendererFactory>();
            if (!rendererFactoryPlugs.empty())
            {
                TBX_ASSERT(rendererFactoryPlugs.size() == 1, "Only one renderer factory is allowed!");
                auto rendererFactory = rendererFactoryPlugs.front();
                AddLayer<RenderingLayer>(rendererFactory, _eventBus);
            }

            auto loggerFactoryPlugs = _pluginServer->GetPlugins<ILoggerFactory>();
            if (!loggerFactoryPlugs.empty())
            {
                TBX_ASSERT(loggerFactoryPlugs.size() == 1, "Only one logger factory is allowed!");
                auto loggerFactory = loggerFactoryPlugs.front();
                AddLayer<LogLayer>(loggerFactory);
            }

            auto inputHandlerPlugs = _pluginServer->GetPlugins<IInputHandler>();
            if (!inputHandlerPlugs.empty())
            {
                TBX_ASSERT(inputHandlerPlugs.size() == 1, "Only one input handler is allowed!");
                auto inputHandler = inputHandlerPlugs.front();
                AddLayer<InputLayer>(inputHandler);
            }

            auto assetLoaderPlugs = _pluginServer->GetPlugins<IAssetLoader>();
            _assetServer = std::make_shared<AssetServer>(workingDirectory, assetLoaderPlugs);
        }

        // Sub to window closing so we can listen for main window closed to init app shutdown
        _eventBus->Subscribe(this, &App::OnWindowClosed);

        // For app inheritors to hook into launch
        OnLaunch();
    }

    void App::Update()
    {
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

        // For app inheritors to hook into update
        OnUpdate();

        for (auto layer : _layerStack)
        {
            layer.Update();
        }

        _eventBus->Post(AppUpdatedEvent());
        _eventBus->ProcessQueue();
    }

    void App::Shutdown()
    {
        TBX_TRACE_INFO("Shutting down...");

        auto hadError = _status == AppStatus::Error;
        auto isRestarting = _status == AppStatus::Reloading;
        _status = AppStatus::Closing;

        // For app inheritors to hook into shutdown
        OnShutdown();

        AppExitingEvent exitEvent;
        _eventBus->Send(exitEvent);
        _eventBus->Unsubscribe(this, &App::OnWindowClosed);

        if (isRestarting)
        {
            _status = AppStatus::Reloading;
        }
        else if (hadError)
        {
            _status = AppStatus::Error;
        }
        else
        {
            _status = AppStatus::Closed;
        }
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

    Ref<WindowManager> App::GetWindowManager() const
    {
        TBX_ASSERT(_windowManager, "Window manager not initialized! Is this app headless?");
        return _windowManager;
    }

    Ref<PluginServer> App::GetPluginServer() const
    {
        return _pluginServer;
    }

    Ref<AssetServer> App::GetAssetServer() const
    {
        return _assetServer;
    }

    void App::RemoveLayer(const Uid& layer)
    {
        _layerStack.Remove(layer);
    }

    void App::OnWindowClosed(const WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        const auto window = e.GetWindow();
        const auto mainWindow = _windowManager->GetMainWindow();
        if (window && mainWindow && window->Id == mainWindow->Id)
        {
            // Stop running and close all windows
            _status = AppStatus::Closing;
        }
    }
}
