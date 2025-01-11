#include "App.h"
#include "Rendering.h"
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
        if (_isRunning) Close();
    }
    
    void App::Launch()
    {
        _isRunning = true;

#ifdef TBX_DEBUG

        // DEBUG:
        
        // Open modules with debug/build path
        PluginServer::LoadPlugins("..\\Build\\bin\\Plugins");

        // No log file in debug
        Log::Open("Tbx::Runtime");

        // Once log is open, we can print out all loaded modules to the log for debug purposes
        const auto& plugins = PluginServer::GetPlugins();
        const auto& numPlugins = plugins.size();
        TBX_INFO("Loaded {0} plugins:", numPlugins);
        ////for (const auto& loadedMod : plugins)
        ////{
        ////    const auto& pluginName = loadedMod.lock()->GetName();
        ////    TBX_INFO("    - {0}", pluginName);
        ////}

#else 

        // RELEASE:
        
        // Open modules with release path
        PluginServer::LoadPlugins("..\\Plugins");

        // Open log file in non-debug
        const auto& currentTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        // TODO: only keep last 10 log files
        const auto& logPath = std::format("Logs\\{}.log", currentTime);
        Log::Open("Tbx::Runtime", logPath);

#endif
        // Subscribe to events:

        OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));
        Rendering::Initialize();
        Rendering::Draw(_mainWindow);
        Input::Initialize();

        OnStart();
    }

    void App::Update()
    {
        for (const auto& window : _windows)
        {
            Rendering::Draw(window);

            Input::SetContext(window);
            window->Update();

            Rendering::Clear();
        }

        for (const auto& layer : _layerStack)
        {
            layer->OnUpdate();
        }

        OnUpdate();

        AppUpdateEvent updateEvent;
        EventDispatcher::Send(updateEvent);
    }

    void App::Close()
    {
        OnShutdown();

        _isRunning = false;
        _mainWindow = nullptr;

        // Call detach on all layers
        for (const auto& layer : _layerStack)
        {
            layer->OnDetach();
        }

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

    void App::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        auto window = CreateNewWindow(name, mode, size);
        _windows.push_back(window);
        if (_mainWindow == nullptr) _mainWindow = window;
    }

    std::shared_ptr<IWindow> App::CreateNewWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        // Create window
        const auto& window = PluginServer::GetPlugin<IWindow>();
        if (window == nullptr)
        {
            TBX_ERROR("Failed to create {0} window. Is a windowing plugin installed?", name);
            return nullptr;
        }

        // Configure and open window
        window->SetTitle(name);
        window->SetSize(size);
        window->Open(mode);

        return window;
    }

    bool App::OnWindowClose(const WindowCloseEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        if (e.GetWindowId() == _mainWindow->GetId()) _isRunning = false;

        // Find the window that was closed and remove it from the list, which will trigger its destruction once nothing no longer references it...
        std::shared_ptr<IWindow> windowToRemove = GetWindow(e.GetWindowId());
        _windows.erase(std::remove(_windows.begin(), _windows.end(), windowToRemove), _windows.end());

        // Don't allow other things to process window close events as the window is about to be destroyed
        return true;
    }

    bool App::OnWindowResize(const WindowResizeEvent& e)
    {
        // Draw the window while its resizing so there are no artifacts during the resize
        std::shared_ptr<IWindow> windowThatWasResized = GetWindow(e.GetWindowId());
        const bool& wasVSyncEnabled = Rendering::IsVSyncEnabled();
        Rendering::SetVSyncEnabled(true); // Enable vsync so the window doesn't flicker
        Rendering::Draw(windowThatWasResized);
        Rendering::SetVSyncEnabled(wasVSyncEnabled); // Set vsync back to what it was

        // Log window resize
        const auto& newSize = windowThatWasResized->GetSize();
        const auto& name = windowThatWasResized->GetTitle();
        TBX_INFO("Window {0} resized to {1}x{2}", name, newSize.Width, newSize.Height);

        // Allow other things to process window resize events
        return false;
    }

    std::weak_ptr<IWindow> App::GetMainWindow() const
    {
        return _mainWindow;
    }

    std::shared_ptr<IWindow> App::GetWindow(const uint64& id)
    {
        for (const auto& window : _windows)
        {
            if (window->GetId() == id)
            {
                return window;
            }
        }

        return nullptr;
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
