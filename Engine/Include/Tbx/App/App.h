#pragma once
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include "Tbx/Graphics/GraphicsSettings.h"
#include <memory>

namespace Tbx
{
    enum class AppStatus
    {
        None = 0,
        Initializing,
        Running,
        Reloading,
        Exiting,
        Closed,
        Error
    };

    class App : public IPlugin, public std::enable_shared_from_this<App>
    {
    public:

        EXPORT explicit(false) App(const std::string_view& name);
        EXPORT ~App() override;

        EXPORT void OnLoad() override;
        EXPORT void OnUnload() override;

        EXPORT void Launch();
        EXPORT void Update();
        EXPORT void Close();

        EXPORT virtual void OnLaunch() = 0;
        EXPORT virtual void OnUpdate() = 0;
        EXPORT virtual void OnShutdown() = 0;

        EXPORT std::shared_ptr<IWindow> GetWindow(Uid id);
        EXPORT const std::vector<std::shared_ptr<IWindow>>& GetWindows();
        EXPORT Uid OpenNewWindow(const std::string& name, const WindowMode& mode, const Size& size);

        /// <summary>
        /// Emplaces a layer into the app.
        /// The app owns this layer and it is destroyed when the app is destroyed.
        /// </summary>
        template <typename T, typename... Args>
        EXPORT std::shared_ptr<T> EmplaceLayer(Args&&... args)
        {
            auto layer = std::make_shared<T>(std::forward<Args>(args)...);
            layer->SetApp(shared_from_this());
            _layerStack.Push(layer);
            return layer;
        }

        EXPORT void PushLayer(const std::shared_ptr<Layer>& layer)
        {
            layer->SetApp(shared_from_this());
            _layerStack.Push(layer);
        }

        EXPORT const AppStatus& GetStatus() const;
        EXPORT const std::string& GetName() const;
        EXPORT std::weak_ptr<IWindow> GetMainWindow() const;

        EXPORT void SetGraphicsSettings(const GraphicsSettings& settings);
        EXPORT const GraphicsSettings& GetGraphicsSettings() const;

    private:
        void OnWindowClosed(const WindowClosedEvent& e);

        std::string _name = "App";
        AppStatus _status = AppStatus::None;
        GraphicsSettings _graphicsSettings = {};

        LayerStack _layerStack = {};
        WindowStack _windowStack = {};

        Uid _mainWindowId = Invalid::Uid;
    };
}
