#include "App.h"
#include "Windowing/IWindow.h"
#include "Modules/Modules.h"
#include "Debug/Debugging.h"
#include "Input/Input.h"
#include <Debug/Log.h>

namespace Toybox
{
    App* App::Instance = nullptr;

    App::App(const std::string& name)
    {
        _name = name;
        _isRunning = false;
        _mainWindow = nullptr;

        delete Instance;
        Instance = this;
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

        // Create main window
        OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));

        // Start handling input
        Input::StartHandling();
    }

    void App::Update()
    {
        _mainWindow->Update();

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

        // Cleanup any remaining windows that are open
        for (auto* window : _windows)
        {
            ((WindowModule*)ModuleServer::GetModule(DefaultWindowModuleName))->DestroyWindow(window);
        }
        
        // Close log and unload modules
        Log::Close();
        ModuleServer::UnloadModules();
    }

    void App::OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        auto* window = CreateNewWindow(name, mode, size);
        _windows.push_back(window);
        if (_mainWindow == nullptr) _mainWindow = window;
    }

    void App::PushLayer(Layer* layer)
    {
        _layerStack.PushLayer(layer);
    }

    void App::PushOverlay(Layer* layer)
    {
        _layerStack.PushOverlay(layer);
    }

    bool App::IsRunning() const
    {
        return _isRunning;
    }

    std::string App::GetName() const
    {
        return _name;
    }

    IWindow* App::GetMainWindow() const
    {
        return _mainWindow;
    }

    IWindow* App::CreateNewWindow(const std::string& name, const WindowMode& mode, const Size& size)
    {
        auto* windowModule = (WindowModule*)ModuleServer::GetModule(DefaultWindowModuleName);
        auto* window = windowModule->CreateNewWindow(name, mode, size);
        window->SetEventCallback(TBX_BIND_EVENT_FN(App::OnEvent));
        return window;
    }

    bool App::OnWindowClose(const WindowCloseEvent& e)
    {
        // If the window is our main window, set running flag to false which will trigger the app to close
        if (e.GetWindowId() == _mainWindow->GetId()) _isRunning = false;

        // Find the window that was closed and destroy it
        for (auto* window : _windows)
        {
            if (e.GetWindowId() == window->GetId())
            {
                ((WindowModule*)ModuleServer::GetModule(DefaultWindowModuleName))->DestroyWindow(window);
            }
        }
        return true;
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
}
