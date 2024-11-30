#include "tbxpch.h"
#include "App.h"
#include "Windowing/IWindow.h"
#include "Modules/Modules.h"

namespace Toybox::Application
{
    App* App::_instance = nullptr;

    App::App(const std::string& name)
    {
        _instance = this;

        _name = name;
        _isRunning = false;
        _mainWindow = nullptr;
    }

    App::~App()
    {
        if (_isRunning) Close();
        delete _mainWindow;
    }

    const App* App::GetInstance()
    {
        return _instance;
    }
    
    void App::Launch()
    {
        _isRunning = true;

        auto* windowModule = (Modules::WindowModule*)Modules::ModuleServer::GetInstance()->GetModule("Glfw Windowing");
        _mainWindow = windowModule->Create();
        _mainWindow->SetEventCallback(TBX_BIND_EVENT_FN(App::OnEvent));
    }

    void App::Update()
    {
        _mainWindow->Update();

        Events::AppUpdateEvent updateEvent;
        OnEvent(updateEvent);

        for (auto it = _layerStack.ReverseBegin(); it != _layerStack.ReverseEnd(); ++it)
        {
            (*it)->OnUpdate();
        }
    }

    void App::Close()
    {
        _isRunning = false;
    }

    void App::PushLayer(Layers::Layer* layer)
    {
        _layerStack.PushLayer(layer);
    }

    void App::PushOverlay(Layers::Layer* layer)
    {
        _layerStack.PushOverlay(layer);
    }

    const bool App::IsRunning() const
    {
        return _isRunning;
    }

    const std::string& App::GetName() const
    {
        return _name;
    }

    Windowing::IWindow* App::GetMainWindow() const
    {
        return _mainWindow;
    }

    bool App::OnWindowClose(Events::WindowCloseEvent& e)
    {
        Close();
        return true;
    }

    void App::OnEvent(Events::Event& e)
    {
        Events::EventDispatcher dispatcher(e);
        dispatcher.Dispatch<Events::WindowCloseEvent>(TBX_BIND_EVENT_FN(App::OnWindowClose));
        //dispatcher.Dispatch<Events::WindowResizeEvent>(TBX_BIND_EVENT_FN(Application::OnWindowResize));

        for (auto it = _layerStack.ReverseBegin(); it != _layerStack.ReverseEnd(); ++it)
        {
            if (e.Handled) break;
            (*it)->OnEvent(e);
        }
    }
}
