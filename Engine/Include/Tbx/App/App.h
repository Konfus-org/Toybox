#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Plugins/PluginServer.h"
#include <memory>

namespace Tbx
{
    /// <summary>
    /// High level lifecycle states the application can transition through while running.
    /// </summary>
    enum class TBX_EXPORT AppStatus
    {
        None,
        Initializing,
        Running,
        Reloading,
        Closing,
        Closed,
        Error
    };

    /// <summary>
    /// Coordinates engine services, manages layers, and drives the lifetime of a Toybox application instance.
    /// </summary>
    class TBX_EXPORT App : public std::enable_shared_from_this<App>
    {
    public:
        App(const std::string_view& name);
        virtual ~App();

        App(const App&) = delete;
        App& operator=(const App&) = delete;
        App(App&&) noexcept = default;
        App& operator=(App&&) noexcept = default;

        /// <summary>
        /// Starts the application run loop and executes until shutdown is requested.
        /// </summary>
        void Run();

        /// <summary>
        /// Requests that the application terminate after the current frame.
        /// </summary>
        void Close();

        // TODO: hide behind methods such as App::SendEvent, App::PostEvent, App::SubscribeToEvent
        EventBus& GetEventBus() const;
        // TODO: hide behind methods such as App::GetPlugins<>, App::GetPlugins, App::AddPlugin<>
        PluginServer& GetPluginServer() const;
        // TODO: hide behind methods such as App::LoadAsset<>, App::GetLoadedAssets, App::AddAsset
        AssetServer& GetAssetServer() const;

        const AppStatus& GetStatus() const;
        const std::string& GetName() const;

        void SetSettings(const Settings& settings);
        const Settings& GetSettings() const;

        template <typename TLayer, typename... TArgs>
        Uid AddLayer(TArgs&&... args)
        {
            const auto& newLayerId = _layerStack.Push<TLayer>(std::forward<TArgs>(args)...);
            const auto& layerName = _layerStack.Get(newLayerId).Name;
            return newLayerId;
        }
        bool HasLayer(const Uid& layerId) const;
        Layer& GetLayer(const Uid& layerId);
        void RemoveLayer(const Uid& layerId);

    protected:
        virtual void OnLaunch() {};
        virtual void OnUpdate() {};
        virtual void OnShutdown() {};

    private:
        void Initialize();
        void Update();
        void Shutdown();
        void OnWindowOpened(const WindowOpenedEvent& e);
        void OnWindowClosed(const WindowClosedEvent& e);

    private:
        std::string _name = "";
        AppStatus _status = AppStatus::None;
        Settings _settings = {};
        LayerStack _layerStack = {};
        Uid _mainWindowId = Uid::Invalid;

        Ref<EventBus> _eventBus = nullptr;
        ExclusiveRef<PluginServer> _pluginServer = nullptr;
        ExclusiveRef<AssetServer> _assetServer = nullptr;
    };
}
