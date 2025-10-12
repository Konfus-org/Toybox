#include "Tbx/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Runtime.h"
#include "Tbx/Graphics/GraphicsBackend.h"
#include "Tbx/Graphics/GraphicsContext.h"
#include "Tbx/Audio/AudioMixer.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Input/Input.h"
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
#include <vector>

namespace Tbx
{
    namespace
    {
        AudioManager CreateAudioManager(const Collection<Ref<Plugin>>& plugins, const Ref<EventBus>& eventBus)
        {
            auto audioMixers = plugins.OfType<IAudioMixer>();
            Ref<IAudioMixer> mixer = audioMixers.empty() ? nullptr : audioMixers.front();
            Ref<EventBus> audioBus = mixer ? eventBus : nullptr;
            return AudioManager(mixer, audioBus);
        }
    }

    App::App(const std::string_view& name, const AppSettings& settings, const Collection<Ref<Plugin>>& plugins, Ref<EventBus> eventBus)
        : Dispatcher(eventBus)
        , Plugins(plugins)
        , Settings(settings)
        , Windowing(Plugins.OfType<IWindowFactory>().front(), Dispatcher)
        , Graphics(Settings.RenderingApi, Plugins.OfType<IGraphicsBackend>(), Plugins.OfType<IGraphicsContextProvider>(), Dispatcher)
        , Audio(CreateAudioManager(plugins, eventBus))
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
        Clock.Reset();
        _frameCount = 0;

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
            Clock.Tick();
            const auto frameDelta = Clock.GetDeltaTime();
            _frameCount++;
            Input::Update();

            // 2. Fixed update
            constexpr float fixedUpdateInterval = 1.0f / 50.0f;
            _fixedUpdateAccumulator += frameDelta.Seconds;
            const auto fixedDelta = DeltaTime(fixedUpdateInterval);
            while (_fixedUpdateAccumulator >= fixedUpdateInterval)
            {
                for (const auto& runtime : Runtimes)
                {
                    runtime->FixedUpdate(fixedDelta);
                }

                OnFixedUpdate(fixedDelta);
                _fixedUpdateAccumulator -= fixedUpdateInterval;
            }

            // 3. Update
            for (const auto& runtime : Runtimes)
            {
                runtime->Update(frameDelta);
            }
            OnUpdate(frameDelta);

            // 4. Late update
            for (const auto& runtime : Runtimes)
            {
                runtime->LateUpdate(frameDelta);
            }
            OnLateUpdate(frameDelta);

            // 5. Render
            Graphics.Render();

            // 6. windows
            Windowing.Update();
          
            // 7. Update audio
            Audio.Update();

            // 8. Dispatch events
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
            // TODO: App slowly eats up more memory every restart meaning we have a leak in our boat! 
            // We need to track down why and fix it!
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
                TBX_TRACE_INFO("App: Gather data...\n");
                DumpFrameReport();
            }
        }
#endif

        // Flush to log at the end of every frame
        Log::Flush();
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
#if defined(_WIN32)
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
        const float epsilon = std::numeric_limits<float>::epsilon();
        const float instantaneousFps = deltaSeconds > epsilon
            ? 1.0f / deltaSeconds
            : 0.0f;
        const int measuredFrames = _frameCount > 0 ? _frameCount - 1 : 0;
        const float averageFps = (accumulatedSeconds > epsilon && measuredFrames > 0)
            ? static_cast<float>(measuredFrames) / accumulatedSeconds
            : instantaneousFps;

        const auto& windows = Windowing.GetAllWindows();
        const size_t windowCount = windows.size();

        std::string mainWindowTitle = "None";
        uint mainWindowWidth = 0u;
        uint mainWindowHeight = 0u;
        float mainWindowAspectRatio = 0.0f;

        if (const auto mainWindow = Windowing.GetMainWindow())
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
        const uint32 renderPassCount = Graphics.GetRenderPasses().size();

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
