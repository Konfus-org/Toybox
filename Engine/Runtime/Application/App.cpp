#include "App.h"
#include "Rendering/Rendering.h"
#include "Time/DeltaTime.h"
#include <TbxCore.h>

namespace Tbx
{
    App::App(const std::string_view& name)
    {
        _name = name;
        _isRunning = false;
        _mainWindow = nullptr;
    }

    App::~App()
    {
        if (_isRunning) 
            ShutdownSystems();
    }
    
    void App::Launch()
    {
        _isRunning = true;

        // Sub to events
        _windowResizeEventId = Events::Subscribe<WindowResizeEvent>(TBX_BIND_CALLBACK(OnWindowResize));

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

#else 

        // RELEASE:
        
        // Open modules with release path
        PluginServer::LoadPlugins("..\\Plugins");

        // Open log file in non-debug
        const auto& currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        // TODO: only keep last 10 log files
        const auto& logPath = std::format("Logs\\{}.log", currentTime);
        Log::Open(logPath);

#endif
        WindowManager::OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));

        Rendering::Initialize();
        Rendering::Draw(_mainWindow);

        Input::Initialize();

        OnStart();
    }

    void App::Update()
    {
        // Update delta time
        Time::DeltaTime::Update();

        // Update windows
        for (const auto& window : _windows)
        {
            Rendering::Draw(window);

            Input::SetContext(window);
            window->Update();

            Rendering::Clear();
        }

        // Then layers
        for (const auto& layer : _layerStack)
        {
            layer->OnUpdate();
        }

        // Call on update for app inheritors
        OnUpdate();

        // Send update event
        AppUpdateEvent updateEvent;
        Events::Send(updateEvent);
    }

    void App::Close()
    {
        OnShutdown();
        ShutdownSystems();
    }

    void App::ShutdownSystems()
    {
        _isRunning = false;

        // Unsub to window events
        Events::Unsubscribe(_windowCloseEventId);
        Events::Unsubscribe(_windowResizeEventId);

        // Call detach on all layers
        for (const auto& layer : _layerStack)
        {
            layer->OnDetach();
        }

        // Close all windows
        for (const auto& window : _windows)
        {
            window->Close();
        }

        // Remove refs to windows to allow them to be destroyed
        _mainWindow.reset();
        _windows.clear();

        Input::Stop();
        Rendering::Shutdown();
        Log::Close();

        // Has to be last! 
        // Everything depends on modules, including the log, input and rendering. 
        // So they cannot be shutdown after modules are unloaded.
        PluginServer::Shutdown();
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

    ////void App::OnWindowClose(WindowCloseEvent& e)
    ////{
    ////    // If the window is our main window, set running flag to false which will trigger the app to close
    ////    if (e.GetWindowId() == _mainWindow->GetId()) _isRunning = false;

    ////    // Find the window that was closed and remove it from the list, which will trigger its destruction once nothing no longer references it...
    ////    std::shared_ptr<IWindow> windowToRemove = GetWindow(e.GetWindowId());
    ////    _windows.erase(std::remove(_windows.begin(), _windows.end(), windowToRemove), _windows.end());

    ////    // Don't allow other things to process window close events as the window is about to be destroyed
    ////    e.Handled = true;
    ////}

    void App::OnWindowResize(const WindowResizeEvent& e)
    {
        // TODO: Move to renderer...

        std::weak_ptr<IWindow> windowThatWasResized = WindowManager::GetWindow(e.GetWindowId());

        // Draw the window while its resizing so there are no artifacts during the resize
        const bool& wasVSyncEnabled = Rendering::IsVSyncEnabled();
        Rendering::SetVSyncEnabled(true); // Enable vsync so the window doesn't flicker
        Rendering::Draw(windowThatWasResized);
        Rendering::SetVSyncEnabled(wasVSyncEnabled); // Set vsync back to what it was

        // Log window resize
        const auto& newSize = windowThatWasResized.lock()->GetSize();
        const auto& name = windowThatWasResized.lock()->GetTitle();
        TBX_INFO("Window {0} resized to {1}x{2}", name, newSize.Width, newSize.Height);
    }

    std::weak_ptr<IWindow> App::GetMainWindow() const
    {
        return _mainWindow;
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
