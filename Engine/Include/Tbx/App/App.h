#pragma once
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Windowing/WindowStack.h"
#include "Tbx/PluginAPI/PluginInterfaces.h"
#include "Tbx/Graphics/GraphicsSettings.h"

namespace Tbx
{
    enum class AppStatus
    {
        None = 0,
        Initializing,
        Running,
        Restarting,
        Exiting,
        Closed,
        Error
    };

    class App : public IPlugin
    {
    public:

        EXPORT explicit(false) App(const std::string_view& name);
        EXPORT ~App() override;

        EXPORT static App* GetInstance();

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
        /// Emplaces a layer into the app. The app owns this layer and it is destroyed when the app is destroyed.
        /// </summary>
        template <typename T, typename... Args>
        EXPORT void EmplaceLayer(Args&&... args)
        {
            auto layer = std::make_shared<T>(std::forward<Args>(args)...);
            if (layer->IsOverlay())
            {
                _sharedLayerStack.PushOverlay(layer);
            }
            else
            {
                _sharedLayerStack.PushLayer(layer);
            }
            layer->OnAttach();
        }

        /// <summary>
        /// Pushes a layer to the app that the app DOES NOT OWN.
        /// It must be kept alive by whomever pushed it.
        /// </summary>
        EXPORT void PushLayer(const std::weak_ptr<Layer>& layer);

        EXPORT const AppStatus& GetStatus() const;
        EXPORT const std::string& GetName() const;
        EXPORT std::weak_ptr<IWindow> GetMainWindow() const;

        EXPORT void SetGraphicsSettings(const GraphicsSettings& settings);
        EXPORT const GraphicsSettings& GetGraphicsSettings() const;

    private:
        void OnWindowClosed(const WindowClosedEvent& e);

        static App* _instance;

        std::string _name = "App";
        AppStatus _status = AppStatus::None;
        GraphicsSettings _graphicsSettings = {};

        SharedLayerStack _sharedLayerStack = {};
        WeakLayerStack _weakLayerStack = {};
        WindowStack _windowStack = {};

        Uid _mainWindowId = Invalid::Uid;

        Uid _windowOpenedEventId = Invalid::Uid;
        Uid _windowClosedEventId = Invalid::Uid;
    };
}
