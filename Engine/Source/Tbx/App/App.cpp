#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Runtime.h"
#include "Tbx/Layers/InputLayer.h"
#include "Tbx/Layers/RenderingLayer.h"
#include "Tbx/Layers/WindowingLayer.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Input/InputCodes.h"
#include "Tbx/Time/DeltaTime.h"
#include "Tbx/Files/Paths.h"

namespace Tbx
{
    App::App(const std::string_view& name, const Settings& settings, const PluginStack& plugins, Ref<EventBus> eventBus)
        : _name(name)
        , _settings(settings)
        , _eventBus(eventBus)
        , _eventListener(eventBus)
        , _plugins(plugins)
    {
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
        TBX_TRACE_INFO("App: Current working directory is: {}", workingDirectory);
        auto assetDirectory = FileSystem::GetAssetDirectory();
        TBX_TRACE_INFO("App: Current asset directory is: {}\n", assetDirectory);

        // Broadcast initial settings
        _eventBus->Send(AppSettingsChangedEvent(_settings));

        // Init core plugin driven systems (if we have the plugins for them)
        // The order in which they are added is the order they are updated
        {
            auto windowFactoryPlugs = _plugins.OfType<IWindowFactory>();
            if (!windowFactoryPlugs.empty())
            {
                TBX_ASSERT(windowFactoryPlugs.size() == 1, "App: Only one window factory is allowed!");
                auto windowFactory = windowFactoryPlugs.front();
                const auto& appName = _name;
                AddLayer<WindowingLayer>(appName, windowFactory, _eventBus);
            }

            auto rendererFactoryPlugs = _plugins.OfType<IRendererFactory>();
            if (!rendererFactoryPlugs.empty())
            {
                TBX_ASSERT(rendererFactoryPlugs.size() == 1, "App: Only one renderer factory is allowed!");
                auto rendererFactory = rendererFactoryPlugs.front();
                AddLayer<RenderingLayer>(rendererFactory, _eventBus);
            }

            auto inputHandlerPlugs = _plugins.OfType<IInputHandler>();
            if (!inputHandlerPlugs.empty())
            {
                TBX_ASSERT(inputHandlerPlugs.size() == 1, "App: Only one input handler is allowed!");
                auto inputHandler = inputHandlerPlugs.front();
                AddLayer<InputLayer>(inputHandler);
            }
        }

        // Sub to window closing so we can listen for main window closed to init app shutdown
        _eventListener.Listen(this, &App::OnWindowOpened);
        _eventListener.Listen(this, &App::OnWindowClosed);

        // Allow other systems to hook into launch
        OnLaunch();
        _eventBus->Send(AppLaunchedEvent(this));

        // Finally, initialize any runtimes
        auto assetLoaderPlugs = _plugins.OfType<IAssetLoader>();
        auto assetServer = MakeRef<AssetServer>(assetDirectory, assetLoaderPlugs);
        for (const auto& layer : _layerStack)
        {
            if (Runtime* runtime = dynamic_cast<Runtime*>(layer.get()))
            {
                runtime->Initialize(assetServer, _eventBus);
            }
        }
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
            TBX_TRACE_INFO("App: Closing...\n");
            _status = AppStatus::Closing;
            return;
        }

        // Shortcut to restart/reload app
        if (Input::IsKeyDown(TBX_KEY_F2))
        {
            // TODO: App slowly eats up more memory every restart, we need to track down why and fix it!
            // There is a leak somewhere...
            TBX_TRACE_INFO("App: Reloading...\n");
            _status = AppStatus::Reloading;
            return;
        }
#endif

        for (const auto& layer : _layerStack)
        {
            layer->Update();
        }

        // Allow other systems to hook into update
        OnUpdate();
        _eventBus->Post(AppUpdatedEvent());
        _eventBus->Flush();
    }

    void App::Shutdown()
    {
        TBX_TRACE_INFO("App: Shutting down...");

        auto hadError = _status == AppStatus::Error;
        auto isRestarting = _status == AppStatus::Reloading;
        _status = AppStatus::Closing;

        // Allow other systems to hook into shutdown
        OnShutdown();
        _eventBus->Post(AppClosedEvent(this));
        _eventBus->Flush();

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

    const AppStatus& App::GetStatus() const
    {
        return _status;
    }

    const std::string& App::GetName() const
    {
        return _name;
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
    
    bool App::HasLayer(const Uid& layerId) const
    {
        return _layerStack.Contains(layerId);
    }

    Layer& App::GetLayer(const Uid& layerId)
    {
        return _layerStack.Get(layerId);
    }

    void App::RemoveLayer(const Uid& layer)
    {
        auto layerName = _layerStack.Get(layer).Name;
        _layerStack.Remove(layer);
    }

    void App::OnWindowOpened(const WindowOpenedEvent& e)
    {
        if (_mainWindowId == Uid::Invalid)
        {
            _mainWindowId = e.GetWindow()->Id;
        }
    }

    void App::OnWindowClosed(const WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        const auto window = e.GetWindow();
        if (window->Id == _mainWindowId)
        {
            // Stop running and close all windows
            _status = AppStatus::Closing;
        }
    }
}
