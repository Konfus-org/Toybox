#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Runtime.h"
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
        TBX_TRACE_INFO("Current working directory is: {}", workingDirectory);
        auto assetDirectory = FileSystem::GetAssetDirectory();
        TBX_TRACE_INFO("Current asset directory is: {}", assetDirectory);

        // Init required systems
        _eventBus = std::make_shared<EventBus>();

#ifdef TBX_SHARED_LIB
        auto self = shared_from_this();
        _pluginServer = std::make_unique<PluginServer>(workingDirectory, _eventBus, self);

        // Try to init plugin driven systems
        // The order in which they are added is the order they are updated
        {
            auto windowFactoryPlugs = _pluginServer->GetPlugins<IWindowFactory>();
            if (!windowFactoryPlugs.empty())
            {
                TBX_ASSERT(windowFactoryPlugs.size() == 1, "Only one window factory is allowed!");
                auto windowFactory = windowFactoryPlugs.front();
                const auto& appName = _name;
                AddLayer<WindowingLayer>(appName, windowFactory, _eventBus);
            }

            auto rendererFactoryPlugs = _pluginServer->GetPlugins<IRendererFactory>();
            if (!rendererFactoryPlugs.empty())
            {
                TBX_ASSERT(rendererFactoryPlugs.size() == 1, "Only one renderer factory is allowed!");
                auto rendererFactory = rendererFactoryPlugs.front();
                AddLayer<RenderingLayer>(rendererFactory, _eventBus);
            }

            auto inputHandlerPlugs = _pluginServer->GetPlugins<IInputHandler>();
            if (!inputHandlerPlugs.empty())
            {
                TBX_ASSERT(inputHandlerPlugs.size() == 1, "Only one input handler is allowed!");
                auto inputHandler = inputHandlerPlugs.front();
                AddLayer<InputLayer>(inputHandler);
            }

            auto loggerFactoryPlugs = _pluginServer->GetPlugins<ILoggerFactory>();
            if (!loggerFactoryPlugs.empty())
            {
                TBX_ASSERT(loggerFactoryPlugs.size() == 1, "Only one logger factory is allowed!");
                auto loggerFactory = loggerFactoryPlugs.front();
                AddLayer<LogLayer>(loggerFactory);
            }

            auto assetLoaderPlugs = _pluginServer->GetPlugins<IAssetLoader>();
            _assetServer = std::make_unique<AssetServer>(assetDirectory, assetLoaderPlugs);
        }
#else
        TBX_TRACE_WARNING(
            "Toybox has been build as a static library, plugins will not be automagically loaded!"
            "You must ensure you link to any wanted plugins in your library and register/setup them in a inheritor of App in its 'OnLaunch' method.");
#endif

        // Sub to window closing so we can listen for main window closed to init app shutdown
        _eventBus->Subscribe(this, &App::OnWindowOpened);
        _eventBus->Subscribe(this, &App::OnWindowClosed);

        // For app inheritors to hook into launch
        OnLaunch();

        // Finally, initialize any runtimes
        for (const auto& layer : _layerStack)
        {
            if (auto runtimePtr = dynamic_cast<Runtime*>(layer.get()))
            {
                runtimePtr->Initialize();
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

        for (const auto& layer : _layerStack)
        {
            layer->Update();
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
        _eventBus->Unsubscribe(this, &App::OnWindowOpened);
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

    const AppStatus& App::GetStatus() const
    {
        return _status;
    }

    const std::string& App::GetName() const
    {
        return _name;
    }

    EventBus& App::GetEventBus() const
    {
        return *_eventBus.get();
    }

    PluginServer& App::GetPluginServer() const
    {
        return *_pluginServer.get();
    }

    AssetServer& App::GetAssetServer() const
    {
        return *_assetServer.get();
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
