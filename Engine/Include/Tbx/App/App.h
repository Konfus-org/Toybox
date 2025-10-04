#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Collections/LayerStack.h"
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
            const PluginContainer& plugins,
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

    public:
        AppStatus Status = AppStatus::None;
        AppSettings Settings = {};
        LayerStack Layers = {};
        PluginContainer Plugins = {};
        Ref<EventBus> Dispatcher = nullptr;

    private:
        std::string _name = "";
        Uid _mainWindowId = Uid::Invalid;
        EventListener _eventListener = {};
    };
}
