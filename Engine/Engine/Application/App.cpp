#include "tbxpch.h"
#include "App.h"
#include "IWindow.h"
#include "GlfwWindow.h"

namespace Toybox::Application
{
    bool _isRunning;
    std::string _name;
    IWindow* _mainWindow;
    Layers::LayerStack _layerStack;

    App::App(const std::string& name)
    {
        _name = name;
        _isRunning = false;
    }

    App::~App()
    {
        if (_isRunning) Close();
        delete _mainWindow;
    }
    
    void App::Launch()
    {
        _isRunning = true;
        _mainWindow = new GlfwWindow(_name, new Math::Size(1920, 1080), WindowMode::Windowed);
        _mainWindow->SetEventCallback(TBX_BIND_EVENT_FN(App::OnEvent));
        OnOpen();
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

        OnUpdate();
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

    IWindow* App::GetMainWindow() const
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
