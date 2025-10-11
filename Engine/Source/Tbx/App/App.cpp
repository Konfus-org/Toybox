#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Runtime.h"
#include "Tbx/Graphics/GraphicsBackend.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Input/Input.h"
#include "Tbx/Input/InputCodes.h"
#include "Tbx/Time/DeltaTime.h"
#include "Tbx/Files/Paths.h"
#include <vector>

namespace Tbx
{
    App::App(const std::string_view& name, const AppSettings& settings, const Collection<Ref<Plugin>>& plugins, Ref<EventBus> eventBus)
        : Dispatcher(eventBus)
        , Plugins(plugins)
        , Settings(settings)
        , Windowing(Plugins.OfType<IWindowFactory>().front(), Dispatcher)
        , Graphics(Settings.RenderingApi, Plugins.OfType<IGraphicsBackend>(), Plugins.OfType<IGraphicsContextProvider>(), Dispatcher)
        , _name(name)
        , _eventListener(eventBus)
    {
        auto inputHandlerPlugs = Plugins.OfType<IInputHandler>();
        Input::SetHandler(inputHandlerPlugs.front());
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

        // 1. Sub to window closing so we can listen for main window closed to init app shutdown
        _eventListener.Listen(this, &App::OnWindowClosed);

        // 2. Allow other systems to hook into launch
        OnLaunch();

        // 3. Init runtimes
        auto assetLoaderPlugs = Plugins.OfType<IAssetLoader>();
        auto assetServer = MakeRef<AssetServer>(assetDirectory, assetLoaderPlugs);
        auto runtimes = Plugins.OfType<Runtime>();
        for (const auto& runtime : runtimes)
        {
            runtime->Initialize(assetServer, Dispatcher);
            Runtimes.Add(runtime);
        }

        // 4. Broadcast app launched events
        Dispatcher->Send(AppSettingsChangedEvent(Settings));
        Dispatcher->Send(AppLaunchedEvent(this));

        // 5. Open main window
#ifdef TBX_DEBUG
        Windowing.OpenWindow(_name, WindowMode::Windowed, Size(1920, 1080));
#else
        Windowing.OpenWindow(_name, WindowMode::Fullscreen);
#endif
    }

    void App::Update()
    {
        // Runtime loop
        {
            // 1. Update delta and input
            DeltaTime::Update();
            Input::Update();

            // 2. Update runtimes
            for (const auto& runtime : Runtimes)
            {
                runtime->Update();
            }

            // 3. Render graphics
            Graphics.Render();

            // 4. Update windows
            Windowing.Update();

            // 5. Allow other systems to hook into update
            OnUpdate();
            Dispatcher->Post(AppUpdatedEvent());
            Dispatcher->Flush();
        }

#ifndef TBX_RELEASE
        // Debugging and development stuff:
        {
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
            // Shortcut to report things like framerate and memory consumption
            else if (Input::IsKeyDown(TBX_KEY_F4))
            {
                // TODO: App slowly eats up more memory every restart, we need to track down why and fix it!
                // There is a leak somewhere...
                TBX_TRACE_INFO("App: Reporting...\n");

                // TODO: Generate frame report!

            }
        }
#endif

        // Flush to log at the end of every frame
        Log::Flush();
    }

    void App::Shutdown()
    {
        TBX_TRACE_INFO("App: Shutting down...");

        auto hadError = Status == AppStatus::Error;
        auto isRestarting = Status == AppStatus::Reloading;
        Status = AppStatus::Closing;

        // Stop listening to input
        Windowing.CloseAllWindows();
        Input::ClearHandler();

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

    void App::OnWindowClosed(const WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        const auto window = e.GetWindow();
        if (window->Id == Windowing.GetMainWindow()->Id)
        {
            // Stop running and close all windows
            Status = AppStatus::Closing;
        }
    }
}
