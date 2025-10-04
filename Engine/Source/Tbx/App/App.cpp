#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Runtime.h"
#include "Tbx/App/Layers/InputLayer.h"
#include "Tbx/App/Layers/RenderingLayer.h"
#include "Tbx/App/Layers/WindowingLayer.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Input/InputCodes.h"
#include "Tbx/Time/DeltaTime.h"
#include "Tbx/Files/Paths.h"
#include <stdexcept>

namespace Tbx
{
    App::App(const std::string_view& name, const AppSettings& settings, const PluginContainer& plugins, Ref<EventBus> eventBus)
        : _name(name)
        , Settings(settings)
        , Dispatcher(eventBus)
        , _eventListener(eventBus)
        , Plugins(plugins)
    {
    }

    App::~App()
    {
        // If we haven't closed yet
        if (Status != AppStatus::Closed &&
            Status != AppStatus::Error)
        {
            Shutdown();
        }
    }

    void App::Run()
    {
        try
        {
            Initialize();

            Status = AppStatus::Running;
            while (Status == AppStatus::Running)
            {
                Update();
            }

            Shutdown();
        }
        catch (const std::exception& ex)
        {
            Status = AppStatus::Error;
            TBX_ASSERT(false, ex.what());
        }

        if (Status != AppStatus::Reloading &&
            Status != AppStatus::Error)
        {
            Status = AppStatus::Closed;
        }
    }

    void App::Close()
    {
        Status = AppStatus::Closing;
    }
    
    void App::Initialize()
    {
        Status = AppStatus::Initializing;

        auto workingDirectory = FileSystem::GetWorkingDirectory();
        TBX_TRACE_INFO("App: Current working directory is: {}", workingDirectory);
        auto assetDirectory = FileSystem::GetAssetDirectory();
        TBX_TRACE_INFO("App: Current asset directory is: {}\n", assetDirectory);

        // Sub to window closing so we can listen for main window closed to init app shutdown
        _eventListener.Listen(this, &App::OnWindowOpened);
        _eventListener.Listen(this, &App::OnWindowClosed);

        // Init core plugin driven systems (if we have the plugins for them)
        // The order in which they are added is the order they are updated
        {
            auto windowFactoryPlugs = Plugins.OfType<IWindowFactory>();
            if (!windowFactoryPlugs.empty())
            {
                TBX_ASSERT(windowFactoryPlugs.size() == 1, "App: Only one window factory is allowed!");
                auto windowFactory = windowFactoryPlugs.front();
                const auto& appName = _name;
                AddLayer<WindowingLayer>(appName, windowFactory, Dispatcher);
            }

            auto rendererFactoryPlugs = Plugins.OfType<IRendererFactory>();
            auto graphicsContextProviders = Plugins.OfType<IGraphicsContextProvider>();
            auto shaderCompilers = Plugins.OfType<IShaderCompiler>();
            if (!rendererFactoryPlugs.empty())
            {
                AddLayer<RenderingLayer>(rendererFactoryPlugs, graphicsContextProviders, shaderCompilers, Dispatcher);
            }

            auto inputHandlerPlugs = Plugins.OfType<IInputHandler>();
            if (!inputHandlerPlugs.empty())
            {
                TBX_ASSERT(inputHandlerPlugs.size() == 1, "App: Only one input handler is allowed!");
                auto inputHandler = inputHandlerPlugs.front();

                // TODO remove layers from app and move to runtimes
                // Then move the existing layers to static plugins and add update logic to plugs...
                AddLayer<InputLayer>(inputHandler);
            }
        }

        // Allow other systems to hook into launch
        OnLaunch();

        // Bind runtimes to this app
        auto assetLoaderPlugs = Plugins.OfType<IAssetLoader>();
        auto assetServer = MakeRef<AssetServer>(assetDirectory, assetLoaderPlugs);
        auto runtimes = Plugins.OfType<Runtime>();
        for (const auto& runtime : runtimes)
        {
            runtime->Initialize(assetServer, Dispatcher);
        }

        // Broadcast app launched events
        Dispatcher->Send(AppSettingsChangedEvent(Settings));
        Dispatcher->Send(AppLaunchedEvent(this));
    }

    void App::Update()
    {
        Time::DeltaTime::Update();

        for (const auto& layer : Layers)
        {
            layer->Update();
        }

        // Allow other systems to hook into update
        OnUpdate();
        Dispatcher->Post(AppUpdatedEvent());
        Dispatcher->Flush();

#ifndef TBX_RELEASE
        // Only allow reloading and force quit when not released!

        // Shortcut to kill the app
        if (Input::IsKeyDown(TBX_KEY_F4) &&
            (Input::IsKeyDown(TBX_KEY_LEFT_ALT) || Input::IsKeyDown(TBX_KEY_RIGHT_ALT)))
        {
            TBX_TRACE_INFO("App: Closing...\n");
            Status = AppStatus::Closing;
        }
        // Shortcut to restart/reload app
        else if (Input::IsKeyDown(TBX_KEY_F2))
        {
            // TODO: App slowly eats up more memory every restart, we need to track down why and fix it!
            // There is a leak somewhere...
            TBX_TRACE_INFO("App: Reloading...\n");
            Status = AppStatus::Reloading;
        }
#endif
    }

    void App::Shutdown()
    {
        TBX_TRACE_INFO("App: Shutting down...");

        auto hadError = Status == AppStatus::Error;
        auto isRestarting = Status == AppStatus::Reloading;
        Status = AppStatus::Closing;

        // Allow other systems to hook into shutdown
        OnShutdown();
        Dispatcher->Post(AppClosedEvent(this));
        Dispatcher->Flush();

        if (isRestarting)
        {
            Status = AppStatus::Reloading;
        }
        else if (hadError)
        {
            Status = AppStatus::Error;
        }
        else
        {
            Status = AppStatus::Closed;
        }
    }

    const AppStatus& App::GetStatus() const
    {
        return Status;
    }

    const std::string& App::GetName() const
    {
        return _name;
    }

    void App::SetSettings(const AppSettings& settings)
    {
        Settings = settings;
        _eventBus->Post(AppSettingsChangedEvent(settings));
    }

    const AppSettings& App::GetSettings() const
    {
        return Settings;
    }

    bool App::HasLayer(const Uid& layerId) const
    {
        return Layers.Any([&layerId](const Ref<Layer>& layer)
        {
            return layer != nullptr && layer->Id == layerId;
        });
    }

    Layer& App::GetLayer(const Uid& layerId)
    {
        for (const auto& layer : Layers)
        {
            if (layer != nullptr && layer->Id == layerId)
            {
                return *layer;
            }
        }

        TBX_ASSERT(false, "App: Requested layer does not exist");
        throw std::runtime_error("App: Requested layer does not exist");
    }

    void App::RemoveLayer(const Uid& layer)
    {
        Layers.Remove(layer);
    }

    const PluginContainer& App::GetPlugins() const
    {
        return Plugins;
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
            Status = AppStatus::Closing;
        }
    }
}
