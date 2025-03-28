#include "Tbx/App/PCH.h"
#include "Tbx/App/App.h"
#include "Tbx/App/Input/Input.h"
#include "Tbx/App/Time/DeltaTime.h"
#include "Tbx/App/Plugins/PluginServer.h"
#include "Tbx/App/Renderer/RenderStack.h"
#include "Tbx/App/Windowing/WindowManager.h"
#include "Tbx/App/Events/ApplicationEvents.h"
#include <Tbx/Core/Events/EventDispatcher.h>

namespace Tbx
{
    App::App(const std::string_view& name)
    {
        _name = name;
        _isRunning = false;
    }

    App::~App()
    {
        if (_isRunning) 
            ShutdownSystems();
    }
    
    void App::Launch(bool headless)
    {
        _isRunning = true;
        _isHeadless = headless;

#ifdef TBX_DEBUG

        // DEBUG:
        
        // Open modules with debug/build path
        PluginServer::LoadPlugins("..\\Build\\bin\\Plugins");

        // No log file in debug
        Log::Open();

        // Once log is open, we can print out all loaded modules to the log for debug purposes
        const auto& plugins = PluginServer::GetLoadedPlugins();
        const auto& numPlugins = plugins.size();
        TBX_INFO("Loaded {0} plugins:", numPlugins);
        for (const auto& loadedMod : plugins)
        {
            const auto& pluginInfo = loadedMod->GetPluginInfo();
            const auto& pluginName = pluginInfo.GetName();
            const auto& pluginVersion = pluginInfo.GetVersion();
            const auto& pluginAuthor = pluginInfo.GetAuthor();
            const auto& pluginDescription = pluginInfo.GetDescription();

            TBX_INFO("{0}:", pluginName);
            TBX_INFO("    - Version: {0}", pluginVersion);
            TBX_INFO("    - Author: {0}", pluginAuthor);
            TBX_INFO("    - Description: {0}", pluginDescription);
        }

        // TODO: only keep last 10 log files in release...
#else 

        // RELEASE:
        
        // Open modules with release path
        PluginServer::LoadPlugins("..\\Plugins");

        // Open log file in non-debug
        const auto& currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        const auto& logPath = std::format("Logs\\{}.log", currentTime);
        Log::Open(logPath);

#endif

        if (!_isHeadless)
        {
            // Subscribe to window events
            _windowClosedEventId = EventDispatcher::Subscribe<WindowClosedEvent>(TBX_BIND_CALLBACK(OnWindowClosed));
            _windowResizeEventId = EventDispatcher::Subscribe<WindowResizedEvent>(TBX_BIND_CALLBACK(OnWindowResize));

            // Open main window
            WindowManager::OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));
            auto mainWindow = WindowManager::GetMainWindow();

            // Init rendering and draw first frame
            RenderStack::Initialize();
            RenderStack::Draw(mainWindow);

            // Init input and set initial context
            Input::Initialize();
            Input::SetContext(mainWindow);
        }

        OnStart();
    }

    void App::Update()
    {
        // Update delta time
        Time::DeltaTime::Update();

        // Call on update for app inheritors
        OnUpdate();

        // Send update event
        AppUpdateEvent updateEvent;
        EventDispatcher::Send(updateEvent);

        // Then update layers
        for (const auto& layer : _layerStack)
        {
            layer->OnUpdate();
        }

        // Finally update windows
        // Needs to be last, as update will trigger events to be processed, and in the case of shutdown, we need to process that at the end of our loop
        if (!_isHeadless)
        {
            // TODO: break this stuff out into their own layers!
            // Windowing layer, rendering layer, input layer...
            
            // Update windows
            for (const auto& window : WindowManager::GetAllWindows())
            {
                RenderStack::Clear();
                RenderStack::Draw(window);

                Input::SetContext(window); // TODO: do this on focus instead...
                window.lock()->Update(); // TODO: do this on update instead...
            }
        }
    }

    void App::Close()
    {
        OnShutdown();
        ShutdownSystems();
    }

    void App::ShutdownSystems()
    {
        _isRunning = false;

        // Unsub to window events and shutdown events
        EventDispatcher::Unsubscribe(_windowClosedEventId);
        EventDispatcher::Unsubscribe(_windowResizeEventId);

        // Call detach on all layers
        for (const auto& layer : _layerStack)
        {
            layer->OnDetach();
        }

        if (!_isHeadless)
        {
            // Close all windows, shutdown rendering and input if we are not headless.
            WindowManager::CloseAllWindows();
            RenderStack::Shutdown();
            Input::Stop();
        }

        // Finally close the log and shutdown events, this should be the last thing to happen before modules are unloaded
        Log::Close();
        EventDispatcher::Clear();

        // Has to be last! 
        // Everything depends on modules, including the log, input and rendering. 
        // So they cannot be shutdown after modules are unloaded.
        PluginServer::Shutdown();
    }

    void App::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size) const
    {
        WindowManager::OpenNewWindow(name, mode, size);
    }

    void App::PushLayer(const std::shared_ptr<Layer>& layer)
    {
        _layerStack.PushLayer(layer);
        layer->OnAttach();
    }

    void App::PushOverlay(const std::shared_ptr<Layer>& layer)
    {
        _layerStack.PushOverlay(layer);
    }

    void App::OnWindowClosed(WindowClosedEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        if (e.GetWindowId() == WindowManager::GetMainWindow().lock()->GetId())
        {
            // Stop running and close all windows
            _isRunning = false;
            WindowManager::CloseAllWindows();
        }
        else
        {
            // Find the window that was closed and remove it from the list, which will trigger its destruction once nothing no longer references it...
            WindowManager::CloseWindow(e.GetWindowId());
        }

        // Don't allow other things to process window close events as the window is about to be destroyed
        e.Handled = true;
    }

    void App::OnWindowResize(const WindowResizedEvent& e)
    {
        std::weak_ptr<IWindow> windowThatWasResized = WindowManager::GetWindow(e.GetWindowId());

        // Draw the window while its resizing so there are no artifacts during the resize
        const bool& wasVSyncEnabled = RenderStack::IsVSyncEnabled();
        RenderStack::SetVSyncEnabled(true); // Enable vsync so the window doesn't flicker
        RenderStack::Draw(windowThatWasResized);
        RenderStack::SetVSyncEnabled(wasVSyncEnabled); // Set vsync back to what it was

        // Log window resize
        const auto& newSize = windowThatWasResized.lock()->GetSize();
        const auto& name = windowThatWasResized.lock()->GetTitle();
        TBX_INFO("Window {0} resized to {1}x{2}", name, newSize.Width, newSize.Height);
    }

    std::weak_ptr<IWindow> App::GetMainWindow() const
    {
        return WindowManager::GetMainWindow();
    }

    const std::string& App::GetName() const
    {
        return _name;
    }

    bool App::IsRunning() const
    {
        return _isRunning;
    }
}
