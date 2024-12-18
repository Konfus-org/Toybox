#pragma once
#include "tbxpch.h"
#include "Windowing/IWindow.h"
#include "Modules/Modules.h"
#include "Layers/Layers.h"
#include "Events/Events.h"

namespace Toybox
{
    TOYBOX_API class App
    {
    public:
        static App* Instance;

        App(const std::string& name);
        virtual ~App();

        void Launch();
        void Update();
        void Close();

        void OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size);
        void PushLayer(Layer* layer);
        void PushOverlay(Layer* layer);

        bool IsRunning() const;
        std::string GetName() const;
        IWindow* GetMainWindow() const;

    private:
        bool _isRunning = false;
        std::string _name = "App";
        IWindow* _mainWindow = nullptr;
        std::vector<IWindow*> _windows;
        LayerStack _layerStack;

        IWindow* CreateNewWindow(const std::string& name, const WindowMode& mode, const Size& size);
        bool OnWindowClose(const WindowCloseEvent& e);
        void OnEvent(Event& e);
    };

    // API to create app, meant to be defined in CLIENT!
    App* CreateApp();
}
