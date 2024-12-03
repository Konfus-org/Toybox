#include "tbxpch.h"
#include "App.h"
#include "Windowing/IWindow.h"
#include "Modules/Modules.h"
#include "Debug/Debugging.h"
#include "Input/Input.h"

namespace Toybox
{
    App* App::Instance = nullptr;

    App::App(const std::string name)
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
        auto* windowModule = (WindowModule*)ModuleServer::GetModule("Glfw Windowing");
        _mainWindow = windowModule->OpenNewWindow(_name, WindowMode::Windowed, Size(1920, 1080));
        _mainWindow->SetEventCallback(TBX_BIND_EVENT_FN(App::OnEvent));

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
        ((WindowModule*)ModuleServer::GetModule("Glfw Windowing"))->DestroyWindow(_mainWindow);
    }

    void App::PushLayer(Layer* layer)
    {
        _layerStack.PushLayer(layer);
    }

    void App::PushOverlay(Layer* layer)
    {
        _layerStack.PushOverlay(layer);
    }

    const bool App::IsRunning() const
    {
        return _isRunning;
    }

    const std::string App::GetName() const
    {
        return _name;
    }

    IWindow* App::GetMainWindow() const
    {
        return _mainWindow;
    }

    bool App::OnWindowClose(WindowCloseEvent& e)
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
