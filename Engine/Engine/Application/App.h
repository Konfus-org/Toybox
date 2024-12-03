#pragma once
#include "tbxpch.h"
#include "Windowing/IWindow.h"
#include "Modules/Modules.h"
#include "Layers/Layers.h"
#include "Events/Events.h"

namespace Toybox
{
    class App
    {
    public:
        static App* Instance;

        App(const std::string& name);
        virtual ~App();

        void Launch();
        void Update();
        void Close();

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

        const bool IsRunning() const;
        const std::string& GetName() const;
        IWindow* GetMainWindow() const;

    private:
        bool _isRunning = false;
        std::string _name = "App";
        IWindow* _mainWindow = nullptr;
        LayerStack _layerStack;

        bool OnWindowClose(WindowCloseEvent& e);
        void OnEvent(Event& e);
    };

    // API to create app, meant to be defined in CLIENT!
    App* CreateApp();
}
