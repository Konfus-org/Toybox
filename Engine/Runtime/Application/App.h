#pragma once
#include <Core.h>
#include "tbxAPI.h"

namespace Toybox
{
    class App : public IApp
    {
    public:
        TBX_API explicit(false) App(const std::string_view& name);
        TBX_API ~App() override;

        TBX_API void Launch() override;
        TBX_API void Update() override;
        TBX_API void Close() override;

        TBX_API void OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size) override;
        TBX_API void PushLayer(const std::shared_ptr<Layer>& layer) override;
        TBX_API void PushOverlay(const std::shared_ptr<Layer>& layer) override;

        TBX_API bool IsRunning() const override;
        TBX_API std::string GetName() const override;
        TBX_API std::weak_ptr<IWindow> GetMainWindow() const override;

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
