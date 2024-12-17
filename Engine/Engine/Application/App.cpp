#include "tbxpch.h"
#include "App.h"
#include "Windowing/IWindow.h"
#include "Debug/Debugging.h"
#include "Input/Input.h"

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

        // Opn log
        Log::Open();

        // Create main window
        OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));

        // Start handling input
        Input::StartHandling();
    }

    void App::Update()
    {
        if (_mainWindow == nullptr) return;

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
        for (auto* window : _windows)
        {
            ((WindowModule*)ModuleServer::GetModule(DefaultWindowModuleName))->DestroyWindow(window);
        }
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
        Close();
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
