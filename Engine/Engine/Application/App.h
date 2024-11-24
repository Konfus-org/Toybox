#pragma once
#include "tbxpch.h"
#include "IWindow.h"
#include "Layers/Layers.h"
#include "Events/Events.h"

namespace Toybox::Application
{
    class App
    {
    public:
        static App* GetInstance();

        App(const std::string& name);
        virtual ~App();

        void Launch();
        void Update();
        void Close();

        void PushLayer(Layers::Layer* layer);
        void PushOverlay(Layers::Layer* layer);

        const bool IsRunning() const;
        const std::string& GetName() const;
        IWindow* GetMainWindow() const;

    private:
        static App* _instance;

        bool _isRunning = false;
        std::string _name = "App";
        IWindow* _mainWindow = nullptr;
        Layers::LayerStack _layerStack;

        bool OnWindowClose(Events::WindowCloseEvent& e);
        void OnEvent(Events::Event& e);
    };

    // API to create app, meant to be defined in CLIENT!
    App* CreateApp();
}
