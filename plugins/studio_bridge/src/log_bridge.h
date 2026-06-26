#pragma once
#include "engine_services.h"
#include "tbx/systems/debugging/log_level.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/typedefs.h"
#include <functional>
#include <string>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: Bridges the engine's logging to the editor. Forwards engine log lines to the editor
    /// over RPC, writes editor-originated lines into the engine's unified log, and applies the
    /// editor's per-level log colours.
    /// @details
    /// Ownership: Owns its registration with the engine log (a listener id). Holds a non-owning
    /// reference to the engine services (for the RPC host). Thread Safety: The forwarding listener
    /// runs on whatever thread logs; the rest are main-thread.
    class LogBridge
    {
      public:
        explicit LogBridge(EngineServices& services);

      public:
        // Registers / removes the engine-log listener that streams lines to the editor.
        void attach();
        void detach();

        /// @brief Writes an editor-originated line into the engine's unified log (no RPC echo back).
        void write_editor_log(const tbx::Json& params);

        /// @brief Applies the editor's per-level log colours (info / warning / error+critical).
        void set_log_colors(const tbx::Json& params);

      private:
        void forward_log(tbx::LogLevel level, const std::string& message);

      private:
        std::reference_wrapper<EngineServices> _services;
        uint _log_listener_id = 0U;
    };
}
