#include "Tbx/PCH.h"
#include "Tbx/Debug/Log.h"
#include "Tbx/App/App.h"
#include "Tbx/Debug/ILogger.h"
#include "Tbx/Debug/StdOutLogger.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/PluginEvents.h"
#include "Tbx/Memory/Refs.h"
#include "Tbx/Plugins/Plugin.h"
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace Tbx
{
    struct QueuedMessage
    {
        LogLevel Level = LogLevel::Info;
        std::string Text = {};
    };

    struct LogState
    {
        EventListener Listener = {};
        std::vector<QueuedMessage> Queue = {};
        Ref<ILogger> Logger = nullptr;
        std::string Name = "Toybox";
        bool Standby = true;
        bool ShuttingDown = false;
        int PluginCount = 0;
    };

    static LogState State;

    static void FlushQueuedMessages()
    {
        if (State.Standby || State.Queue.empty()) return;

        for (auto& entry : State.Queue)
        {
            State.Logger->Write(static_cast<int>(entry.Level), entry.Text);
        }
        State.Logger->Flush();
        State.Queue.clear();
    }

    static void ResetState()
    {
        State.Logger = nullptr;
        State.Queue.clear();
        State.Name = "";
        State.Standby = true;
        State.ShuttingDown = false;
        State.PluginCount = 0;
    }

    static void OnPluginLoaded(const PluginLoadedEvent& e)
    {
        auto plugin = e.plugin;
        if (auto logger = std::dynamic_pointer_cast<ILogger>(plugin.lock()))
            State.Logger = logger;
        else
            State.PluginCount++;
    }

    static void OnPluginUnloaded(PluginUnloadedEvent&)
    {
        if (State.PluginCount > 0) State.PluginCount--;
        else if (State.ShuttingDown) ResetState();
    }

    static void OnAppLaunched(const AppLaunchedEvent& e)
    {
        if (!State.Standby)
        {
            TBX_ASSERT(false, "Log: Received app launch while logger state was already active, resetting state.");
            ResetState();
        }

        State.Name = e.LaunchedApp->GetName();
        if (State.Name.empty()) State.Name = "Tbx";

        State.Standby = false;
        if (State.Logger) State.Logger->Open(State.Name, "");
        else State.Logger = MakeRef<StdOutLogger>();

        FlushQueuedMessages();
    }

    static void OnAppClosed(AppClosedEvent&)
    {
        State.ShuttingDown = true;
        if (State.Logger) State.Logger->Flush();
        if (State.PluginCount == 0) ResetState();
    }

    static void EnsureListenerBound()
    {
        static bool bound = false;
        if (bound) return;

        auto bus = EventBus::Global;
        if (!bus)
        {
            TBX_ASSERT(false, "Log: Missing global event bus.");
            return;
        }

        State.Listener.Bind(bus);
        State.Listener.Listen<PluginLoadedEvent>(OnPluginLoaded);
        State.Listener.Listen<PluginUnloadedEvent>(OnPluginUnloaded);
        State.Listener.Listen<AppLaunchedEvent>(OnAppLaunched);
        State.Listener.Listen<AppClosedEvent>(OnAppClosed);
        bound = true;
    }

    void Log::Flush()
    {
        State.Standby = false;
        FlushQueuedMessages();
    }

    void Log::Trace(LogLevel level, std::string message)
    {
        if (State.Standby)
        {
            if (State.Queue.empty())
            {
                EnsureListenerBound();
                std::printf("Log: In standby mode, queuing log messages until app launch or manual flush.\n\n");
            }

            State.Queue.push_back({ level, message });
            return;
        }
        State.Logger->Write(static_cast<int>(level), message);
    }
}
