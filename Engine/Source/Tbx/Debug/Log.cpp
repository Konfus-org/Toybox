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
        EventListener listener = {};
        std::vector<QueuedMessage> queue = {};
        Ref<ILogger> logger = nullptr;
        std::string channelName = "Toybox";
        bool standby = true;
        bool shuttingDown = false;
        int pluginCount = 0;
    };

    static LogState& GetState()
    {
        static LogState state;
        return state;
    }

    static void FlushQueuedMessages(LogState& state)
    {
        if (state.standby || state.queue.empty())
        {
            return;
        }

        if (state.logger)
        {
            for (auto& entry : state.queue)
            {
                state.logger->Write(static_cast<int>(entry.Level), entry.Text);
            }
            state.logger->Flush();
        }
        else
        {
            for (auto& entry : state.queue)
            {
                std::fputs(entry.Text.c_str(), stdout);
            }
        }

        state.queue.clear();
    }

    static void CloseCurrentLogger(LogState& state)
    {
        if (!state.logger)
        {
            return;
        }

        state.logger->Flush();
        state.logger->Close();
        state.logger = nullptr;
    }

    static void ResetState(LogState& state)
    {
        CloseCurrentLogger(state);
        state.queue.clear();
        state.channelName = "Toybox";
        state.standby = true;
        state.shuttingDown = false;
        state.pluginCount = 0;
    }

    static void ActivateFallbackLogger(LogState& state)
    {
        if (state.logger)
        {
            if (dynamic_cast<StdOutLogger*>(state.logger.get()) != nullptr)
            {
                if (!state.standby)
                {
                    state.logger->Open(state.channelName, "");
                    FlushQueuedMessages(state);
                }
                return;
            }

            CloseCurrentLogger(state);
        }

        auto fallback = MakeRef<StdOutLogger>();
        state.logger = fallback;

        if (!state.standby)
        {
            state.logger->Open(state.channelName, "");
            FlushQueuedMessages(state);
        }

        std::printf("Toybox: Using stdout logger.\n");
    }

    static void SwapLogger(LogState& state, const Ref<ILogger>& nextLogger)
    {
        if (state.logger == nextLogger)
        {
            return;
        }

        CloseCurrentLogger(state);
        state.logger = nextLogger;

        if (state.logger && !state.standby)
        {
            state.logger->Open(state.channelName, "");
            FlushQueuedMessages(state);
        }
    }

    static Ref<ILogger> ExtractLogger(const Ref<Plugin>& plugin)
    {
        if (!plugin)
        {
            return nullptr;
        }

        if (auto* logger = dynamic_cast<ILogger*>(plugin.get()))
        {
            return Ref<ILogger>(plugin, logger);
        }

        return nullptr;
    }

    static void HandlePluginLoaded(const Ref<Plugin>& plugin)
    {
        if (!plugin)
        {
            return;
        }

        auto& state = GetState();
        state.pluginCount++;

        auto logger = ExtractLogger(plugin);
        if (!logger)
        {
            return;
        }

        SwapLogger(state, logger);
    }

    static void HandlePluginUnloaded(const Ref<Plugin>& plugin)
    {
        if (!plugin)
        {
            return;
        }

        auto& state = GetState();
        if (state.pluginCount > 0)
        {
            state.pluginCount--;
        }
        else
        {
            TBX_ASSERT(false, "Log: Plugin count underflow.");
            state.pluginCount = 0;
        }

        auto logger = ExtractLogger(plugin);
        if (logger && state.logger && state.logger.get() == logger.get())
        {
            CloseCurrentLogger(state);

            if (!state.shuttingDown)
            {
                ActivateFallbackLogger(state);
            }
        }

        if (state.shuttingDown && state.pluginCount == 0)
        {
            ResetState(state);
        }
    }

    static void OnPluginLoaded(PluginLoadedEvent& event)
    {
        HandlePluginLoaded(event.GetPlugin().lock());
    }

    static void OnPluginsLoaded(PluginsLoadedEvent& event)
    {
        for (const auto& plugin : event.GetLoadedPlugins())
        {
            HandlePluginLoaded(plugin);
        }
    }

    static void OnPluginUnloaded(PluginUnloadedEvent& event)
    {
        HandlePluginUnloaded(event.GetPlugin().lock());
    }

    static void OnPluginsUnloaded(PluginsUnloadedEvent& event)
    {
        for (const auto& plugin : event.GetUnloadedPlugins())
        {
            HandlePluginUnloaded(plugin);
        }
    }

    static void OnAppLaunched(AppLaunchedEvent& event)
    {
        auto& state = GetState();
        if (!state.standby)
        {
            TBX_ASSERT(false, "Log: Received app launch while logger state was already active, resetting state.");
            ResetState(state);
        }

        state.channelName = event.GetApp().GetName();
        if (state.channelName.empty())
        {
            state.channelName = "Toybox";
        }

        state.standby = false;

        if (state.logger)
        {
            state.logger->Open(state.channelName, "");
        }
        else
        {
            ActivateFallbackLogger(state);
        }

        FlushQueuedMessages(state);
    }

    static void OnAppClosed(AppClosedEvent&)
    {
        auto& state = GetState();
        state.shuttingDown = true;

        if (state.logger)
        {
            state.logger->Flush();
        }

        if (state.pluginCount == 0)
        {
            ResetState(state);
        }
    }

    static void EnsureListenerBound()
    {
        static bool bound = false;
        if (bound)
        {
            return;
        }

        auto& state = GetState();
        auto bus = EventBus::Global;
        if (!bus)
        {
            TBX_ASSERT(false, "Log: Missing global event bus.");
            state.standby = false;
            ActivateFallbackLogger(state);
            bound = true;
            return;
        }

        state.listener.Bind(bus);
        state.listener.Listen<PluginLoadedEvent>(OnPluginLoaded);
        state.listener.Listen<PluginsLoadedEvent>(OnPluginsLoaded);
        state.listener.Listen<PluginUnloadedEvent>(OnPluginUnloaded);
        state.listener.Listen<PluginsUnloadedEvent>(OnPluginsUnloaded);
        state.listener.Listen<AppLaunchedEvent>(OnAppLaunched);
        state.listener.Listen<AppClosedEvent>(OnAppClosed);
        bound = true;
    }

    void Log::Trace(LogLevel level, std::string message)
    {
        EnsureListenerBound();

        auto& state = GetState();
        if (state.standby)
        {
            state.queue.push_back({ level, std::move(message) });
            return;
        }

        if (!state.logger && !state.shuttingDown)
        {
            ActivateFallbackLogger(state);
        }

        if (state.logger)
        {
            state.logger->Write(static_cast<int>(level), message);
            return;
        }

        std::fputs(message.c_str(), stdout);
    }
}
