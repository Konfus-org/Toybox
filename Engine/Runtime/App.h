#pragma once
#include <TbxCore.h>

namespace Tbx
{
    class App
    {
    public:
        TBX_API explicit(false) App(const std::string_view& name);
        TBX_API ~App();

        TBX_API void Launch();
        TBX_API void Update();
        TBX_API void Close();

        TBX_API virtual void OnStart() = 0;
        TBX_API virtual void OnUpdate() = 0;
        TBX_API virtual void OnShutdown() = 0;

        TBX_API void OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size);
        TBX_API void PushLayer(const std::shared_ptr<Layer>& layer);
        TBX_API void PushOverlay(const std::shared_ptr<Layer>& layer);

        TBX_API bool IsRunning() const;
        TBX_API const std::string& GetName() const;
        TBX_API std::weak_ptr<IWindow> GetMainWindow() const;

    private:
        bool _isRunning = false;
        std::string _name = "App";
        std::shared_ptr<IWindow> _mainWindow;
        std::vector<std::shared_ptr<IWindow>> _windows;
        LayerStack _layerStack;

        std::shared_ptr<IWindow> CreateNewWindow(const std::string& name, const WindowMode& mode, const Size& size);
        bool OnWindowClose(const WindowCloseEvent& e);
        bool OnWindowResize(const WindowResizeEvent& e);
        std::shared_ptr<IWindow> GetWindow(const uint64& id);
        void OnEvent(Event& e);
    };
}
