#pragma once
#include "Tbx/App/Settings.h"
#include "Tbx/Assets/AssetServer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventCarrier.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/WindowEvents.h"
#include "Tbx/Windowing/WindowManager.h"
#include "Tbx/Graphics/GraphicsManager.h"
#include "Tbx/Audio/AudioManager.h"
#include "Tbx/Plugins/Plugin.h"
#include "Tbx/Collections/Queryable.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Time/Chronometer.h"
#include "Tbx/Time/DeltaTime.h"

namespace Tbx
{
    class Runtime;

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

    class TBX_EXPORT App
    {
    public:
        App(const std::string_view& name,
            const AppSettings& settings,
            const Queryable<Ref<Plugin>>& plugins,
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
        virtual void OnFixedUpdate(const DeltaTime&) {};
        virtual void OnUpdate(const DeltaTime&) {};
        virtual void OnLateUpdate(const DeltaTime&) {};
        virtual void OnShutdown() {};

    private:
        void Initialize();
        void Update();
        void Shutdown();
        void OnWindowClosed(const WindowClosedEvent& e);

    public:
        Ref<EventBus> Bus = nullptr;
        Queryable<Ref<Plugin>> Plugins = {};
        Queryable<Ref<Runtime>> Runtimes = {};
        ExclusiveRef<IWindowManager> Windowing = {};
        ExclusiveRef<IGraphicsManager> Graphics = {};
        ExclusiveRef<IAudioManager> Audio = {};
        ExclusiveRef<AssetServer> Assets = {};
        AppStatus Status = AppStatus::None;
        AppSettings Settings = {};
        Chronometer Clock = {};

    private:
        std::string _name = "";
        EventCarrier _carrier = {};
        EventListener _listener = {};
        float _fixedUpdateAccumulator = 0.0f;
        // TODO: move this elsewhere! Perhaps a plugin?
        void DumpFrameReport() const;
        bool _captureDebugData = false;
        int _frameCount = 0;
    };
}
