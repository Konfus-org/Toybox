#pragma once
#include <Core.h>
#include "tbxAPI.h"

namespace Toybox
{
    class TBX_API App
    {
    public:
        static App* Instance;

        explicit(false) App(const std::string& name);
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
}
