#pragma once
#include "tbx/systems/files/json.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct SelectionState;
    struct ViewState;

    /// @brief Replaces the selection from an ids array (malformed entries are ignored): clears the
    /// runtime selected tag from the previously selected entities and stamps it on the new set, so
    /// the engine's tag-gated selection-outline post effect tracks the editor's selection. An id may
    /// live in the active world or in an asset-preview view's world; each id's world is resolved.
    void apply_selection(
        SelectionState& selection,
        const EngineServices& services,
        ViewState& views,
        const tbx::Json& ids_array);
}
