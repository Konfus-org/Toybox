#pragma once
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct LogState;

    /// @brief Registers the engine-log listener that streams every engine line to the editor as an
    /// engine.log notification. @p services must outlive the listener (it is the plugin's own bundle,
    /// removed by detach_log before the plugin unloads).
    void attach_log(LogState& log, const EngineServices& services);

    /// @brief Removes the engine-log listener.
    void detach_log(LogState& log);

    /// @brief Writes an editor-originated line into the engine's unified log (no RPC echo back).
    void write_editor_log(const tbx::Json& params);

    /// @brief Applies the editor's per-level log colours (info / warning / error+critical).
    void set_log_colors(const tbx::Json& params);
}
