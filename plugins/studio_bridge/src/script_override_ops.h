#pragma once
#include "tbx/systems/files/json.h"
#include "tbx/utils/result.h"

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct ViewState;

    // The per-field script-override sync ops, routed from sync_path_ops for the address
    // world/{w}/entities/{id}/components/script_container/scripts/{bindingId}/overrides/{field}.
    // These are what keep the persisted override blob LEAN: a set stores just the field's typed
    // { type, value } (or erases it again when the value equals the script's default), so the
    // attribute-enriched full-field-set the describe pass expands (see enrich_script_overrides)
    // never round-trips back into the binding. While playing, a write also lands on the live
    // instance through ScriptSystem::apply_overrides, so a mid-play tweak takes effect without
    // restarting the script. Main-thread only (request handling).

    /// @brief Sets one override field ({ bindingId, property, value }) on an entity's script
    /// binding; storing the lean typed value, or erasing the override when the value equals the
    /// script's default. A reset is just a set back to the script's default (the editor computes the
    /// default from the enriched blob and writes it), so there is no separate reset verb.
    Result set_script_override(
        const EngineServices& services, ViewState& views, const tbx::Json& params);
}
