#pragma once
#include <Core.h>
#include "tbxAPI.h"

namespace Toybox
{
    // TODO: move application to core! Or at least an interface with the singelton pattern so we can access it from the modules.
    class App
    {
    public:
        TBX_API static App* Instance;

        TBX_API explicit(false) App(const std::string_view& name);
        TBX_API virtual ~App();

        TBX_API void Launch();
        TBX_API void Update();
        TBX_API void Close();

        TBX_API void OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size);
        TBX_API void PushLayer(Layer* layer);
        TBX_API void PushOverlay(Layer* layer);

        TBX_API bool IsRunning() const;
        TBX_API std::string GetName() const;
        TBX_API std::weak_ptr<IWindow> GetMainWindow() const;

    private:
        bool _isRunning = false;
        std::string _name = "App";
        std::shared_ptr<IWindow> _mainWindow;
        std::vector<std::shared_ptr<IWindow>> _windows;
        LayerStack _layerStack;

        std::shared_ptr<IWindow> CreateNewWindow(const std::string& name, const WindowMode& mode, const Size& size);
        bool OnWindowClose(const WindowCloseEvent& e);
        void OnEvent(Event& e);
    };
}
