#pragma once
#include "data_plane_layout.h"
#include "shared_file_mapping.h"
#include "tbx/types/typedefs.h"
#include <array>
#include <string>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The engine-side drain bookkeeping of one view slot's input lane: the last consumed
    /// seqlock sequence (skip unchanged cells) and the last consumed cumulative mouse/wheel totals
    /// (their diff is the exact accumulated delta, however many editor writes were coalesced).
    struct SlotDrainState
    {
        uint32 last_sequence = 0U;
        double last_dx = 0.0;
        double last_dy = 0.0;
        double last_wheel = 0.0;
    };

    /// @brief
    /// Purpose: The editor data plane — the file-backed shared-memory mapping whose lanes carry the
    /// hot editor traffic (input, projections, camera poses, draw commands) beside the JSON-RPC
    /// control plane. Plain state: data_plane_ops owns the behavior (create/destroy, the hello
    /// advert, and view-slot assignment).
    /// @details
    /// Ownership: Owned by the plugin by value. Owns the mapping and (through destroy_data_plane)
    /// the backing temp file. Thread Safety: Created and destroyed on the main thread; slot
    /// assignment is main-thread only. The mapped cells carry their own cross-process seqlock
    /// synchronization.
    struct DataPlaneState
    {
        SharedFileMapping mapping = {};
        // The mapped layout, or null while the plane is unavailable (creation failed or not yet
        // requested). Points into `mapping`.
        DataPlaneShm* shm = nullptr;
        // Stamped into a slot at assignment so a stale writer (a view stopped and its slot reused)
        // is ignored by the consumers. Never 0 — 0 marks a slot that was never assigned.
        uint32 next_generation = 1U;
        // Set once creation has been attempted, so a failed create is logged/attempted only once.
        bool create_attempted = false;

        // Which view owns each slot (empty = free) — assigned at view.start, released at view.stop /
        // disconnect. Kept engine-side (not in the mapping) because only this process assigns slots.
        std::array<std::string, DATA_PLANE_MAX_VIEWS> slot_views = {};
        std::array<SlotDrainState, DATA_PLANE_MAX_VIEWS> drain = {};

        // Stamped into each published projection/camera cell so the editor can skip a snapshot it
        // has already consumed. One tick per publish pass (engine frame).
        uint64 frame_counter = 0U;
    };
}
