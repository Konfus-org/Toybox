#pragma once
#include "tbx/types/typedefs.h"
#include <atomic>
#include <type_traits>

namespace tbx::studio_bridge
{
    /// @brief
    /// Purpose: The wire layout of the editor data plane — a file-backed shared-memory mapping the
    /// editor opens by path (advertised in the editor.hello reply). This header is the single source
    /// of truth for the layout; the editor hand-mirrors the cell payloads (DataPlaneLayout.cs) and
    /// validates every offset/stride/size the DataPlaneHeaderShm publishes at open, so any drift is
    /// caught at runtime instead of corrupting lanes silently.
    /// @details
    /// Every cell is a single-writer/single-reader seqlock: the writer bumps `sequence` to odd,
    /// writes the payload, then stores it back even (release); the reader copies the payload between
    /// two even, equal reads of `sequence`. All lanes are latest-value-wins — no rings, no history.
    /// The mapping's bytes start zeroed (a fresh zero-filled file), which is the valid initial state
    /// for every cell (sequence 0 = even/idle, active 0 = free).

    inline constexpr uint32 DATA_PLANE_MAGIC = 0x44584254U; // 'TBXD' little-endian
    inline constexpr uint32 DATA_PLANE_LAYOUT_VERSION = 1U;
    inline constexpr uint32 DATA_PLANE_MAX_VIEWS = 16U;
    inline constexpr uint32 DATA_PLANE_MAX_PROJECTED_ENTITIES = 1024U;
    inline constexpr uint32 DATA_PLANE_DRAW_LAYER_COUNT = 16U;
    inline constexpr uint32 DATA_PLANE_DRAW_PAYLOAD_CAPACITY = 256U * 1024U;

    // Capability bits: which lanes the engine actively serves. The layout always reserves every
    // region; a clear bit just means nobody writes that lane yet.
    namespace DataPlaneCapabilities
    {
        inline constexpr uint32 INPUT = 0x1U;
        inline constexpr uint32 PROJECTION = 0x2U;
        inline constexpr uint32 CAMERA = 0x4U;
        inline constexpr uint32 DRAW = 0x8U;
    }

    // DrawLayerCellShm::flags bits.
    namespace DrawLayerFlags
    {
        inline constexpr uint32 DEPTH_TEST = 0x1U;
        inline constexpr uint32 OVERFLOWED = 0x2U;
    }

    // In DrawLayerCellShm::view_slot: the layer draws in every editor view.
    inline constexpr uint32 DRAW_LAYER_ALL_VIEWS = 0xFFFFFFFFU;

    /// @brief
    /// Purpose: The draw lane's command vocabulary. Every command starts with a DrawCmdHeaderShm;
    /// payloads are packed little-endian floats/uints in the field order documented per kind, and
    /// each command's `size` (header included) is a multiple of 8 so successors stay aligned. The
    /// vocabulary maps 1:1 onto tbx::Gizmos methods — the decoder is a dumb replayer, so the bridge
    /// carries no editor semantics.
    namespace DrawCommandKind
    {
        inline constexpr uint16 SET_COLOR = 0x00U;      // rgba[4]
        inline constexpr uint16 SET_MATRIX = 0x01U;     // m[16] C# row-major; decode transposes
        inline constexpr uint16 RESET_MATRIX = 0x02U;   // (no payload)
        inline constexpr uint16 LINE = 0x10U;           // a[3], b[3]
        inline constexpr uint16 RAW_LINES = 0x11U;      // u32 vertex_count, u32 rsvd, GizmoVertex[]
        inline constexpr uint16 RAW_TRIANGLES = 0x12U;  // u32 vertex_count, u32 rsvd, GizmoVertex[]
        inline constexpr uint16 WIRE_BOX = 0x20U;       // center[3], size[3], quat[4] (xyzw)
        inline constexpr uint16 WIRE_SPHERE = 0x21U;    // center[3], radius
        inline constexpr uint16 WIRE_CAPSULE = 0x22U;   // center[3], radius, half_height, quat[4]
        inline constexpr uint16 RING = 0x23U;           // center[3], axis[3], radius
        inline constexpr uint16 WIRE_SQUARE = 0x24U;    // center[3], size[2], quat[4]
        inline constexpr uint16 WIRE_PLANE = 0x25U;     // center[3], normal[3], size
        inline constexpr uint16 ARROW = 0x26U;          // from[3], to[3]
        inline constexpr uint16 AXES = 0x27U;           // origin[3], quat[4], size
        inline constexpr uint16 SOLID_BOX = 0x30U;      // center[3], size[3], quat[4]
        inline constexpr uint16 SOLID_SPHERE = 0x31U;   // center[3], radius
        inline constexpr uint16 SOLID_SQUARE = 0x32U;   // center[3], size[2], quat[4]
        inline constexpr uint16 SOLID_PLANE = 0x33U;    // center[3], normal[3], size
        inline constexpr uint16 SOLID_ARROW = 0x34U;    // from[3], to[3], rgba[4], shaft_radius
        inline constexpr uint16 SOLID_BEAM = 0x35U;     // from[3], to[3], rgba[4], radius
        inline constexpr uint16 SOLID_CYLINDER = 0x36U; // from[3], to[3], radius, rgba[4]
        inline constexpr uint16 SOLID_CONE = 0x37U;     // base[3], tip[3], radius, rgba[4]
        inline constexpr uint16 SOLID_TORUS = 0x38U;    // center[3], axis[3], ring_r, tube_r, rgba[4]
        inline constexpr uint16 FILLED_ARC = 0x39U;     // center[3], axis[3], radius, start[3], sweep, rgba[4]
        inline constexpr uint16 MESH = 0x40U;   // u64 mesh, u64 material, m[16] (C# row-major), rgba[4]
        inline constexpr uint16 SPRITE = 0x41U; // u64 texture, center[3], size[2] (w, h), rgba[4]
    }

    // DrawCmdHeaderShm (mesh/sprite) flag bits.
    namespace DrawCommandFlags
    {
        // The command's mesh id is a builtin ordinal (1 cube, 2 sphere, 3 capsule, 4 half-sphere,
        // 5 quad, 6 triangle) instead of an asset id.
        inline constexpr uint16 MESH_BUILTIN = 0x1U;
    }

    /// @brief
    /// Purpose: The 8-byte head of every draw command. `size` is the total byte count including
    /// this header, always a multiple of 8.
    struct DrawCmdHeaderShm
    {
        uint16 kind = 0U;
        uint16 flags = 0U;
        uint32 size = 0U;
    };

    /// @brief
    /// Purpose: One editor input snapshot (Studio → engine, latest wins). Mirrors the view.input RPC
    /// payload, except mouse/wheel movement is carried as cumulative totals since view start instead
    /// of per-message deltas: overwritten snapshots then lose nothing — the engine's drain diffs the
    /// totals against the last-consumed values to recover the exact accumulated delta.
    struct InputSnapshotShm
    {
        uint32 buttons = 0U;   // ViewButtons bit set (LEFT / RIGHT / MIDDLE)
        uint32 move_keys = 0U; // Editor fly-camera move-key bitset
        uint32 focused = 0U;   // bool
        uint32 key_count = 0U; // Live entries in keys
        int32 keys[32] = {};   // Held keys as tbx::InputKey codes
        float mouse_x = 0.0F;
        float mouse_y = 0.0F;
        float cursor_u = 0.0F; // Normalized cursor in the rendered image (top-left origin)
        float cursor_v = 0.0F;
        double total_dx = 0.0;
        double total_dy = 0.0;
        double total_wheel = 0.0;
    };

    /// @brief
    /// Purpose: One projected entity (engine → Studio): where an entity sits on a view's screen.
    /// Mirrors the view.projectEntities reply items.
    struct ProjectedEntityShm
    {
        uint64 id = 0U; // tbx::Uuid value
        float u = 0.0F; // Normalized screen position (top-left origin)
        float v = 0.0F;
        float depth = 0.0F;
        uint32 flags = 0U; // Reserved
    };

    /// @brief
    /// Purpose: The input lane cell of one view slot. `generation` guards slot reuse: the editor
    /// stamps the generation it was handed at view.start, and the engine's drain ignores a mismatch.
    struct InputCellShm
    {
        std::atomic<uint32> sequence = {};
        uint32 generation = 0U;
        InputSnapshotShm snapshot = {};
    };

    /// @brief
    /// Purpose: The projection lane cell of one view slot: the latest full snapshot of entity screen
    /// positions for that view, published once per engine frame while the view is active.
    struct ProjectionCellShm
    {
        std::atomic<uint32> sequence = {};
        uint32 count = 0U;
        uint32 overflow_count = 0U; // Entities dropped because the snapshot exceeded capacity
        uint32 flags = 0U;          // Reserved
        uint64 frame_index = 0U;
        ProjectedEntityShm items[DATA_PLANE_MAX_PROJECTED_ENTITIES] = {};
    };

    /// @brief
    /// Purpose: The camera lane cell of one view slot: the pose/projection the view renders with
    /// this frame, published beside the projection lane from the same CameraView — so editor-side
    /// projection math reproduces the engine's bit-for-bit. Matrices are row-major glm values stored
    /// column-contiguous exactly as glm lays them out in memory (m[column][row], 16 floats).
    struct CameraCellShm
    {
        std::atomic<uint32> sequence = {};
        uint32 is_orthographic = 0U; // bool
        uint32 width = 0U;           // Render-target size in pixels
        uint32 height = 0U;
        float view_projection[16] = {};
        float view[16] = {};
        float projection[16] = {};
        float position[3] = {};
        float pad0 = 0.0F;
        float rotation[4] = {}; // Quaternion (x, y, z, w)
        float z_near = 0.0F;
        float z_far = 0.0F;
        float fov_degrees = 0.0F;
        float aspect = 0.0F;
        uint64 frame_index = 0U;
    };

    /// @brief
    /// Purpose: One view's lanes. `generation` is stamped by the engine when the slot is assigned at
    /// view.start and handed to the editor in the reply; `active` marks the slot as owned by a live
    /// view. Cache-line aligned cells so the two processes' writers never share a line.
    struct ViewSlotShm
    {
        std::atomic<uint32> generation = {};
        uint32 active = 0U;
        alignas(64) InputCellShm input = {};
        alignas(64) ProjectionCellShm projection = {};
        alignas(64) CameraCellShm camera = {};
    };

    /// @brief
    /// Purpose: One editor draw layer (Studio → engine): a packed command buffer the editor rewrites
    /// wholesale each editor tick and the engine decodes once per sequence change. Layer slots are
    /// allocated editor-side (the engine is a pure reader); sequence 0 means the layer is absent.
    struct DrawLayerCellShm
    {
        std::atomic<uint32> sequence = {};
        uint32 size_bytes = 0U; // Valid bytes in payload
        uint32 view_slot = DRAW_LAYER_ALL_VIEWS;
        uint32 flags = 0U;      // DrawLayerFlags
        uint64 tick_index = 0U;
        uint32 command_count = 0U;
        uint32 reserved = 0U;
        uint8 payload[DATA_PLANE_DRAW_PAYLOAD_CAPACITY] = {};
    };

    /// @brief
    /// Purpose: The self-describing mapping header. The engine fills every offset/stride/size from
    /// the real struct layout (offsetof/sizeof) at create; the editor refuses the plane if any value
    /// disagrees with its own mirrored expectations.
    struct DataPlaneHeaderShm
    {
        uint32 magic = 0U;
        uint32 layout_version = 0U;
        uint32 capabilities = 0U;
        uint32 total_size = 0U;
        uint32 max_views = 0U;
        uint32 max_projected_entities = 0U;
        uint32 slots_offset = 0U;
        uint32 slot_stride = 0U;
        uint32 input_cell_offset = 0U;
        uint32 input_cell_size = 0U;
        uint32 projection_cell_offset = 0U;
        uint32 projection_cell_size = 0U;
        uint32 camera_cell_offset = 0U;
        uint32 camera_cell_size = 0U;
        uint32 projected_entry_size = 0U;
        uint32 draw_table_offset = 0U;
        uint32 draw_layer_count = 0U;
        uint32 draw_layer_stride = 0U;
        uint32 draw_payload_capacity = 0U;
        uint32 reserved[5] = {};
    };

    /// @brief
    /// Purpose: The whole mapping. The file is exactly sizeof(DataPlaneShm) bytes.
    struct DataPlaneShm
    {
        DataPlaneHeaderShm header = {};
        alignas(64) ViewSlotShm slots[DATA_PLANE_MAX_VIEWS] = {};
        alignas(64) DrawLayerCellShm draw_layers[DATA_PLANE_DRAW_LAYER_COUNT] = {};
    };

    // The seqlock protocol only works when the counters are plain lock-free words, and zeroed file
    // bytes must be a valid initial state for every cell.
    static_assert(std::atomic<uint32>::is_always_lock_free);
    static_assert(sizeof(std::atomic<uint32>) == 4U);

    // The editor mirrors these payloads by hand; any size change here is a layout-version bump.
    static_assert(std::is_trivially_copyable_v<InputSnapshotShm>);
    static_assert(std::is_trivially_copyable_v<ProjectedEntityShm>);
    static_assert(sizeof(InputSnapshotShm) == 184U);
    static_assert(sizeof(ProjectedEntityShm) == 24U);
    static_assert(sizeof(InputCellShm) == 192U);
    static_assert(sizeof(ProjectionCellShm) == 24600U);
    static_assert(sizeof(CameraCellShm) == 264U);
    static_assert(sizeof(DrawLayerCellShm) == 32U + DATA_PLANE_DRAW_PAYLOAD_CAPACITY);
    static_assert(offsetof(DrawLayerCellShm, payload) == 32U);
    static_assert(offsetof(ViewSlotShm, input) == 64U);
    static_assert(offsetof(ViewSlotShm, projection) == 256U);
    static_assert(sizeof(DataPlaneHeaderShm) == 96U);
    static_assert(offsetof(DataPlaneShm, slots) == 128U);
}
