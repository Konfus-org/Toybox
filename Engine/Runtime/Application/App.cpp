#include "App.h"
#include "Windowing/IWindow.h"
#include "Modules/Modules.h"
#include "Debug/Debugging.h"
#include "Input/Input.h"
#include <Debug/Log.h>

namespace Toybox
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

        // Load modules
        ModuleServer::LoadModules();

        // Open log
        Log::Open();

#ifdef TBX_DEBUG
        // Once log is open, we can print out all loaded modules to the log for debug purposes
        const auto& modules = ModuleServer::GetModules();
        const auto& numModules = modules.size();
        TBX_INFO("Loaded {0} modules:", numModules);
        for (const auto& loadedMod : modules)
        {
            const auto& modName = loadedMod.lock()->GetName();
            TBX_INFO("    - {0}", modName);
        }
#endif

        // Create main window
        OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));

        // Start handling input
        Input::StartHandling(_mainWindow);
    }

    void App::Update()
    {
        if (_mainWindow != nullptr)
        {
            _mainWindow->Update();
        }

        AppUpdateEvent updateEvent;
        OnEvent(updateEvent);

        for (auto it = _layerStack.ReverseBegin(); it != _layerStack.ReverseEnd(); ++it)
        {
            if (!_isRunning) return;
            (*it)->OnUpdate();
        }
    }

    void App::Close()
    {
        _isRunning = false;
        _mainWindow = nullptr;

        // We will immediately stop handling input
        Input::StopHandling();
        
        // Close log and unload modules
        Log::Close();
        ModuleServer::UnloadModules();
    }

    void App::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<WindowCloseEvent>(TBX_BIND_EVENT_FN(App::OnWindowClose));
        //dispatcher.Dispatch<Events::WindowResizeEvent>(TBX_BIND_EVENT_FN(Application::OnWindowResize));

        for (auto it = _layerStack.ReverseBegin(); it != _layerStack.ReverseEnd(); ++it)
        {
            if (e.Handled) break;
            (*it)->OnEvent(e);
        }
    }

    void App::PushLayer(Layer* layer)
    {
        _layerStack.PushLayer(layer);
    }

    void App::PushOverlay(Layer* layer)
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
        auto windowFactory = ModuleServer::GetFactoryModule<IWindow>();
        if (!Toybox::IsWeakPointerValid(windowFactory))
        {
            TBX_ERROR("Failed to create window because the window {0}, because the window factory couldn't be found...", name);
            return nullptr;
        }

        auto sharedWindow = windowFactory.lock()->CreateShared();
        sharedWindow->SetTitle(name);
        sharedWindow->SetSize(size);
        sharedWindow->Open(mode);
        sharedWindow->SetEventCallback(TBX_BIND_EVENT_FN(App::OnEvent));
        return sharedWindow;
    }

    bool App::OnWindowClose(const WindowCloseEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        if (e.GetWindowId() == _mainWindow->GetId()) _isRunning = false;

        // Find the window that was closed and remove it from the list, which will trigger its destruction once nothing no longer references it...
        std::shared_ptr<IWindow> windowToRemove;
        for (auto window : _windows)
        {
            if (e.GetWindowId() == window->GetId())
            {
                windowToRemove = window;
                break;
            }
        }
        _windows.erase(std::remove(_windows.begin(), _windows.end(), windowToRemove), _windows.end());

        return true;
    }

    std::weak_ptr<IWindow> App::GetMainWindow() const
    {
        return _mainWindow;
    }

    std::string App::GetName() const
    {
        return _name;
    }

    bool App::IsRunning() const
    {
        return _isRunning;
    }
}
