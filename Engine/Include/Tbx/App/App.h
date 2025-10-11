#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/App/Runtime.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Graphics/GraphicsManager.h"
#include "Tbx/Collections/Collection.h"
#include "Tbx/Memory/Refs.h"

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
            const Collection<Ref<Plugin>>& plugins,
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
        void OnWindowClosed(const WindowClosedEvent& e);
        void DumpFrameReport() const;

    public:
        // TODO: Make a proper dispatcher and the bus will live seperately and just keep track of subscriptions...
        // it won't deal with dispatching or posting or anything like that.
        Ref<EventBus> Dispatcher = nullptr;
        Collection<Ref<Plugin>> Plugins = {};
        Collection<Ref<Runtime>> Runtimes = {};
        AppStatus Status = AppStatus::None;
        AppSettings Settings = {};
        WindowManager Windowing = {};
        GraphicsManager Graphics = {};

    private:
        std::string _name = "";
        EventListener _eventListener = {};
    };
}
