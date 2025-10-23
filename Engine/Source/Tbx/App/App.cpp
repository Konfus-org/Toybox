#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Runtime.h"
#include "Tbx/Graphics/GraphicsBackend.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Audio/AudioMixer.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Debug/Log.h"
#include "Tbx/Input/HeadlessInputHandler.h"
#include "Tbx/Input/IInputHandler.h"
#include "Tbx/Input/InputCodes.h"
#include "Tbx/Time/Chronometer.h"
#include "Tbx/Time/DeltaTime.h"
#include "Tbx/Files/Paths.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <thread>
#include <vector>

namespace Tbx
{
    App* App::_instance = nullptr;

    App::App(
        const std::string_view& name,
        const AppSettings& settings,
        const Queryable<Ref<Plugin>>& plugins,
        const Queryable<Ref<Runtime>>& runtimes,
        Ref<EventBus> eventBus)
        : Bus(eventBus)
        , Plugins(plugins)
        , Runtimes(runtimes)
        , Settings(settings)
        , _name(name)
        , _carrier(Bus)
        , _listener(Bus)
    {
        TBX_ASSERT(Bus, "App: Requires a valid event bus instance.");
        TBX_ASSERT(!_instance, "App: Existing singleton was replaced, unexpected behavior may occur.");
        _instance = this;
    }

    App::~App()
    {
        // If we haven't closed yet
        if (Status != AppStatus::Closed &&
            Status != AppStatus::Reloading &&
            Status != AppStatus::Error)
        {
            Shutdown();
        }

        if (_instance == this)
        {
            _instance = nullptr;
        }

    }

    const std::string& App::GetName() const
    {
        return _name;
    }

    App* App::GetInstance()
    {
        return _instance;
    }

    bool App::IsRunning() const
    {
        return Status == AppStatus::Running || 
            Status == AppStatus::Paused || 
            Status == AppStatus::Minimized;
    }

    void App::Run()
    {
        try
        {
            Initialize();

            Status = AppStatus::Running;
            while (IsRunning())
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
        Clock.Reset();
        _frameCount = 0;
      
        if (IsDebugBuild)
        {
            TBX_TRACE_INFO("App: Using debug build.\n");
        }
        else
        {
            TBX_TRACE_INFO("App: Using release build.\n");
        }

        // 1. Init core systems
        {
            // Input
            auto inputHandlerPlugs = Plugins.OfType<IInputHandler>();
            if (!inputHandlerPlugs.Any())
            {
                TBX_TRACE_WARNING("App: No input handler plugins detected, running headless input.");
                Input = MakeRef<HeadlessInputHandler>();
            }
            else
            {
                if (inputHandlerPlugs.Count() != 1) TBX_TRACE_WARNING("App: Multiple input handler plugins detected, only one is allowed. Using first detected.");
                Input = inputHandlerPlugs.First();
            }

            // Windowing
            auto windowFactories = Plugins.OfType<IWindowFactory>();
            if (!windowFactories.Any())
            {
                TBX_TRACE_WARNING("App: No window factories detected, running headless windowing.");
                Windowing = MakeRef<HeadlessWindowManager>();
            }
            else
            {
                if (windowFactories.Count() != 1) TBX_TRACE_WARNING("App: Multiple window factory plugins detected, only one is allowed. Using first detected.");
                Windowing = MakeRef<WindowManager>(windowFactories.First(), Input, Bus);
            }

            // Graphics
            auto graphicsBackends = Plugins.OfType<IGraphicsBackend>();
            auto graphicsContextProviders = Plugins.OfType<IGraphicsContextProvider>();
            if (!graphicsBackends.Any() || !graphicsContextProviders.Any())
            {
                TBX_TRACE_WARNING("App: Graphics backends and context providers not detected, running headless graphics.");
                Graphics = MakeRef<HeadlessGraphicsManager>();
                Settings.RenderingApi = GraphicsApi::None;
            }
            else
            {
                Graphics = MakeRef<GraphicsManager>(
                    Settings.RenderingApi,
                    graphicsBackends.Vector(),
                    graphicsContextProviders.Vector(),
                    Bus);
            }

            // Audio
            auto audioMixers = Plugins.OfType<IAudioMixer>();
            if (!audioMixers.Any())
            {
                TBX_TRACE_WARNING("App: Audio mixers not detected, running headless audio.");
                Audio = MakeRef<HeadlessAudioManager>();
            }
            else
            {
                if (audioMixers.Count() != 1) TBX_TRACE_WARNING("App: Multiple audio mixer plugins detected, only one is allowed. Using first detected.");
                Audio = MakeRef<AudioManager>(audioMixers.First(), Bus);
            }

            // Assets
            auto assetLoaders = Plugins.OfType<IAssetLoader>();
            if (!assetLoaders.Any())
            {
                TBX_TRACE_WARNING("App: No asset loader plugins detected, asset server will be non-functional.");
            }
            Assets = MakeRef<AssetServer>(FileSystem::GetAssetDirectory(), assetLoaders.Vector());
        }

        // 2. Sub to window closing so we can listen for main window closed to init app shutdown
        _listener.Listen<WindowClosedEvent>([this](WindowClosedEvent& event)
        {
            OnWindowClosed(event);
        });
        _listener.Listen<WindowModeChangedEvent>([this](WindowModeChangedEvent& event)
        {
            OnWindowModeChanged(event);
        });

        // 3. Allow other systems to hook into launch
        OnLaunch();

        // 4. Init runtimes
        for (const auto& runtime : Runtimes)
        {
            runtime->OnStart(this);
        }

        // 5. Broadcast app launched events
        _carrier.Send(AppLaunchedEvent(this));
        _carrier.Send(AppSettingsChangedEvent({}, Settings));

        // 6. Open main window
        if (IsDebugBuild)
        {
            Windowing->OpenWindow(_name, WindowMode::Windowed, Size(1920, 1080));
        }
        else
        {
            Windowing->OpenWindow(_name, WindowMode::Fullscreen);
        }

        TBX_TRACE_INFO("App: Initialized!.\n");
    }

    void App::Update()
    {
        TBX_TRACE_VERBOSE("App: Starting update!.\n");

        // Paused or minimized loop
        {
            if (Status == AppStatus::Paused)
            {
                Input->Update();
                Windowing->Update();
                Audio->Update();
                Bus->Flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return;
            }
            else if (Status == AppStatus::Minimized)
            {
                Windowing->Update();
                Bus->Flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                return;
            }
        }

        // Check for settings changes
        // TODO: find a better way to do this!
        if (Settings.Vsync != _lastFramesSettings.Vsync ||
            Settings.RenderingApi != _lastFramesSettings.RenderingApi ||
            Settings.Resolution.Width != _lastFramesSettings.Resolution.Width ||
            Settings.Resolution.Height != _lastFramesSettings.Resolution.Height ||
            Settings.ClearColor.R != _lastFramesSettings.ClearColor.R ||
            Settings.ClearColor.G != _lastFramesSettings.ClearColor.G ||
            Settings.ClearColor.B != _lastFramesSettings.ClearColor.B ||
            Settings.ClearColor.A != _lastFramesSettings.ClearColor.A)
        {
            _carrier.Send(AppSettingsChangedEvent(_lastFramesSettings, Settings));
        }
        _lastFramesSettings = Settings;

        // Check for status changes
        // TODO: find a better way to do this!
        if (Status != _lastFrameStatus)
        {
            _carrier.Send(AppStatusChangedEvent(_lastFrameStatus, Status));
        }
        _lastFrameStatus = Status;

        // Runtime loop, order is important here!
        {
            // Update delta and input
            Clock.Tick();
            const auto frameDelta = Clock.GetDeltaTime();
            _frameCount++;
            Input->Update();

            // Fixed update
            constexpr float fixedUpdateInterval = 1.0f / 50.0f;
            _fixedUpdateAccumulator += frameDelta.Seconds;
            const auto fixedDelta = DeltaTime(fixedUpdateInterval);
            while (_fixedUpdateAccumulator >= fixedUpdateInterval)
            {
                for (const auto& runtime : Runtimes)
                {
                    runtime->OnFixedUpdate(fixedDelta);
                }

                OnFixedUpdate(fixedDelta);
                _fixedUpdateAccumulator -= fixedUpdateInterval;
            }

            // Update
            for (const auto& runtime : Runtimes)
            {
                runtime->OnUpdate(frameDelta);
            }
            OnUpdate(frameDelta);

            // Late update
            for (const auto& runtime : Runtimes)
            {
                runtime->OnLateUpdate(frameDelta);
            }
            OnLateUpdate(frameDelta);

            // Graphics
            Graphics->Update();

            // Windows
            Windowing->Update();
          
            // Update audio
            Audio->Update();

            // Dispatch events
            _carrier.Post(AppUpdatedEvent(this));
            Bus->Flush();
        }

        // Shortcut to kill the app
        if (Input->IsKeyDown(TBX_KEY_F4) &&
            (Input->IsKeyDown(TBX_KEY_LEFT_ALT) || Input->IsKeyDown(TBX_KEY_RIGHT_ALT)))
        {
            TBX_TRACE_INFO("App: Closing...\n");
            Status = AppStatus::Closing;
        }

#ifndef TBX_RELEASE
        // Debugging and development stuff:
        {
            // Shortcut to restart/reload app
            if (Input->IsKeyDown(TBX_KEY_F2))
            {
                // TODO: App slowly eats up more memory every restart meaning we have a leak in our boat!
                // We need to track down why and fix it!
                TBX_TRACE_INFO("App: Reloading...\n");
                Status = AppStatus::Reloading;
            }
            // Shortcut to report things like framerate and memory consumption
            else if (Input->IsKeyDown(TBX_KEY_F4))
            {
                TBX_TRACE_INFO("App: Gather data...\n");
                DumpFrameReport();
            }
        }
#endif

        TBX_TRACE_VERBOSE("App: Ending update!.\n");
    }

    void App::Shutdown()
    {
        TBX_TRACE_INFO("App: Shutting down...\n");

        auto hadError = Status == AppStatus::Error;
        auto isRestarting = Status == AppStatus::Reloading;
        Status = AppStatus::Closing;

        // Allow other systems to hook into shutdown
        OnShutdown();
        _carrier.Send(AppClosedEvent(this));

        // Shudown runtimes
        for (const auto& runtime : Runtimes)
        {
            runtime->OnShutdown();
        }

        // Cleanup
        Runtimes = {};
        Plugins = {};
        Assets = nullptr;
        Bus->Flush();

        // Update status
        if (isRestarting) Status = AppStatus::Reloading;
        else if (hadError) Status = AppStatus::Error;
        else Status = AppStatus::Closed;
    }

    void App::OnWindowClosed(const WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        const auto* window = e.AffectedWindow;
        if (window->Id == Windowing->GetMainWindow()->Id)
        {
            // Stop running and close all windows
            Status = AppStatus::Closing;
        }
    }

    void App::OnWindowModeChanged(const WindowModeChangedEvent& e)
    {
        bool allWindowsMinimized = true;
        for (auto window : Windowing->GetAllWindows())
        {
            if (window->GetMode() != WindowMode::Minimized)
            {
                allWindowsMinimized = false;
                break;
            }
        }
        if (allWindowsMinimized)
        {
            // Minimize the app
            Status = AppStatus::Minimized;
            _carrier.Send(AppStatusChangedEvent(_lastFrameStatus, Status));
        }
        else
        {
            Status = _lastFrameStatus;
            _carrier.Send(AppStatusChangedEvent(_lastFrameStatus, Status));
        }
    }
    
    void App::DumpFrameReport() const
    {
        const DeltaTime frameDelta = Clock.GetDeltaTime();
        const float deltaSeconds = frameDelta.Seconds;
        const float deltaMilliseconds = frameDelta.Milliseconds;
        const float clockDeltaSeconds = frameDelta.Seconds;
        const float clockDeltaMilliseconds = frameDelta.Milliseconds;
        const auto accumulatedSeconds = Clock.GetAccumulatedTime().count();
        const auto accumulatedMilliseconds = accumulatedSeconds * 1000.0f;
        const auto systemTimePoint = Clock.GetSystemTime();
        const auto systemTimeMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(systemTimePoint.time_since_epoch()).count();

        std::string systemTimeString = "Unavailable";
        if (systemTimePoint.time_since_epoch() != Chronometer::SystemClock::duration::zero())
        {
            const auto systemTimeT = Chronometer::SystemClock::to_time_t(systemTimePoint);
            std::tm systemTimeTm{};
#if defined(TBX_PLATFORM_WINDOWS)
            localtime_s(&systemTimeTm, &systemTimeT);
#else
            localtime_r(&systemTimeT, &systemTimeTm);
#endif
            std::ostringstream systemTimeStream;
            systemTimeStream << std::put_time(&systemTimeTm, "%Y-%m-%d %H:%M:%S");
            const auto systemTimeSubSecond = std::chrono::duration_cast<std::chrono::milliseconds>(systemTimePoint.time_since_epoch() % std::chrono::seconds(1));
            systemTimeStream << '.' << std::setw(3) << std::setfill('0') << systemTimeSubSecond.count();
            systemTimeString = systemTimeStream.str();
        }
        constexpr float epsilon = std::numeric_limits<float>::epsilon();
        const float instantaneousFps = deltaSeconds > epsilon
            ? 1.0f / deltaSeconds
            : 0.0f;
        const int measuredFrames = _frameCount > 0 ? _frameCount - 1 : 0;
        const float averageFps = (accumulatedSeconds > epsilon && measuredFrames > 0)
            ? static_cast<float>(measuredFrames) / accumulatedSeconds
            : instantaneousFps;

        const auto& windows = Windowing->GetAllWindows();
        const size_t windowCount = windows.size();

        std::string mainWindowTitle = "None";
        uint mainWindowWidth = 0u;
        uint mainWindowHeight = 0u;
        float mainWindowAspectRatio = 0.0f;

        if (const auto mainWindow = Windowing->GetMainWindow())
        {
            mainWindowTitle = mainWindow->GetTitle();
            const auto& size = mainWindow->GetSize();
            mainWindowWidth = size.Width;
            mainWindowHeight = size.Height;
            if (mainWindowHeight != 0u)
            {
                mainWindowAspectRatio = static_cast<float>(mainWindowWidth) / static_cast<float>(mainWindowHeight);
            }
        }

        const uint64 pluginCount = Plugins.Count();
        const uint64 runtimeCount = Runtimes.Count();
        const uint32 renderPassCount = Graphics->GetRenderPasses().size();

        const auto vsyncToString = [](VsyncMode mode) -> std::string_view
        {
            switch (mode)
            {
                case VsyncMode::Off: return "Off";
                case VsyncMode::On: return "On";
                case VsyncMode::Adaptive: return "Adaptive";
                default: return "Unknown";
            }
        };

        const auto graphicsApiToString = [](GraphicsApi api) -> std::string_view
        {
            switch (api)
            {
                case GraphicsApi::None: return "None";
                case GraphicsApi::Vulkan: return "Vulkan";
                case GraphicsApi::OpenGL: return "OpenGL";
                case GraphicsApi::Metal: return "Metal";
                case GraphicsApi::Custom: return "Custom";
                default: return "Unknown";
            }
        };

        TBX_TRACE_INFO(
            "App: Frame Report\n"
            "\tFrame time: {:.3f} ms ({:.3f} s)\n"
            "\tFPS (instant): {:.2f}\n"
            "\tFPS (average): {:.2f}\n"
            "\tClock delta: {:.3f} ms ({:.3f} s)\n"
            "\tClock runtime: {:.3f} ms ({:.3f} s)\n"
            "\tClock system time: {} ({} ms since epoch)\n"
            "\tOpen windows: {}\n"
            "\tMain window: {} ({}x{}, aspect {:.2f})\n"
            "\tRender passes: {}\n"
            "\tPlugins: {}\n"
            "\tRuntimes: {}\n"
            "\tGraphics API: {}\n"
            "\tVSync: {}\n",
            deltaMilliseconds,
            deltaSeconds,
            instantaneousFps,
            averageFps,
            clockDeltaMilliseconds,
            clockDeltaSeconds,
            accumulatedMilliseconds,
            accumulatedSeconds,
            systemTimeString,
            systemTimeMilliseconds,
            windowCount,
            mainWindowTitle,
            mainWindowWidth,
            mainWindowHeight,
            mainWindowAspectRatio,
            renderPassCount,
            pluginCount,
            runtimeCount,
            graphicsApiToString(Settings.RenderingApi),
            vsyncToString(Settings.Vsync));
    }
}
