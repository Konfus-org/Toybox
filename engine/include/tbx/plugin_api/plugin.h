#pragma once
#include "tbx/application_context.h"
#include "tbx/time/delta_time.h"
#include "tbx/events/event.h"
#include "tbx/commands/command.h"

namespace tbx
{
    class Plugin
    {
    public:
        virtual ~Plugin() = default;

        // Called when the plugin is attached to the host
        virtual void on_attach(const ApplicationContext& context) = 0;

        // Called before the plugin is detached from the host
        virtual void on_detach() = 0;

        // Per-frame update with delta timing
        virtual void on_update(const DeltaTime& dt) = 0;

        // Broadcast or targeted events from the host
        virtual void on_event(const Event& evt) = 0;

        // Console or programmatic commands routed to the plugin
        virtual void on_command(const Command& cmd) = 0;
    };
}
