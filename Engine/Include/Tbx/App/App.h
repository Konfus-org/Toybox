#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Layers/LayerStack.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Plugins/PluginLoader.h"

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
    class TBX_EXPORT App
    {
    public:
        App(const std::string_view& name,
            const AppSettings& settings,
            const PluginCollection& plugins,
            Ref<EventBus> eventBus);
        virtual ~App();

        /// <summary>
        /// Starts the application run loop and executes until shutdown is requested.
        /// </summary>
        void Run();

        /// <summary>
        /// Requests that the application terminate after the current frame.
        /// </summary>
        void Close();

        const AppStatus& GetStatus() const;
        const std::string& GetName() const;

        void SetSettings(const AppSettings& settings);
        const AppSettings& GetSettings() const;

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
        AppSettings _settings = {};
        LayerStack _layerStack = {};
        Uid _mainWindowId = Uid::Invalid;
        Ref<EventBus> _eventBus = nullptr;
        EventListener _eventListener = {};
        PluginCollection _plugins = {};
    };
}
