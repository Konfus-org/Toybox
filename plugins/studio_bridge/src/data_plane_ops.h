#pragma once
#include "data_plane_state.h"
#include "tbx/systems/files/json.h"
#include "tbx/types/typedefs.h"
#include "tbx/utils/result.h"
#include <string>

namespace tbx::studio_bridge
{
    struct EngineServices;
    struct ViewState;

    // Creates the plane's backing temp file (named per engine pid + rpc port so every launch is a
    // fresh file and a crashed predecessor's file is never reopened), maps it, and initializes the
    // self-describing header. Idempotent: an already-created plane (or an already-failed attempt)
    // returns without retrying.
    Result ensure_data_plane(DataPlaneState& plane, uint16 rpc_port);

    // Unmaps and deletes the backing file.
    void destroy_data_plane(DataPlaneState& plane);

    bool is_data_plane_available(const DataPlaneState& plane);

    // The editor.hello reply's advert: { path, layoutVersion, size }, or null when unavailable.
    tbx::Json describe_data_plane(const DataPlaneState& plane);

    // Assigns a free view slot to @p view_name: stamps a fresh generation, marks it active, and
    // zeroes its cells' bookkeeping. Returns the slot index, or -1 when every slot is taken (the
    // view then runs on the RPC fallback).
    int32 acquire_view_slot(DataPlaneState& plane, const std::string& view_name);

    // Releases @p view_name's slot at view stop. A no-op for a view without one.
    void release_view_slot(DataPlaneState& plane, const std::string& view_name);

    // Releases every slot (editor disconnect / teardown).
    void release_all_view_slots(DataPlaneState& plane);

    // The generation stamped at the slot's last acquire (0 for an invalid slot).
    uint32 view_slot_generation(const DataPlaneState& plane, int32 slot);

    // Drains every active slot's input lane into its view's ViewInput (the same struct the
    // view.input RPC fallback fills): state fields overwrite, mouse/wheel movement accumulates from
    // the cumulative-total diff. Called at the top of the bridge update, before any input consumer.
    void drain_input_lanes(DataPlaneState& plane, ViewState& views);

    // Publishes each active view's projection + camera cells (the engine → editor lanes) from the
    // same camera pose this frame renders with — the push that replaces the editor's
    // view.projectEntities polling. A view that has settled idle (idle_settle_frames == 0) skips
    // publishing: nothing on screen moved, so the editor keeps the previous snapshot. Called right
    // after sync_camera_entities (which computes the activity signal and the rendered pose).
    void publish_view_lanes(DataPlaneState& plane, ViewState& views, const EngineServices& services);
}
