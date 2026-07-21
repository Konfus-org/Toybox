#include "data_plane_ops.h"
#include "engine_services.h"
#include "view_input.h"
#include "view_state.h"
#include "wire.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/screen_projection.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <format>
#include <glm/gtc/type_ptr.hpp>
#include <mutex>
#include <system_error>

#ifdef _WIN32
    #include <process.h>
#else
    #include <unistd.h>
#endif

namespace tbx::studio_bridge
{
    static uint32 current_process_id()
    {
#ifdef _WIN32
        return static_cast<uint32>(::_getpid());
#else
        return static_cast<uint32>(::getpid());
#endif
    }

    // Fills the self-describing header from the real struct layout, so the editor validates against
    // what this binary actually mapped rather than what its own constants hope for.
    static void initialize_header(DataPlaneShm& shm)
    {
        auto& header = shm.header;
        header.magic = DATA_PLANE_MAGIC;
        header.layout_version = DATA_PLANE_LAYOUT_VERSION;
        // Lanes light up as their producers/consumers land; the layout always reserves the regions.
        header.capabilities = DataPlaneCapabilities::INPUT | DataPlaneCapabilities::PROJECTION
                              | DataPlaneCapabilities::CAMERA | DataPlaneCapabilities::DRAW;
        header.total_size = static_cast<uint32>(sizeof(DataPlaneShm));
        header.max_views = DATA_PLANE_MAX_VIEWS;
        header.max_projected_entities = DATA_PLANE_MAX_PROJECTED_ENTITIES;
        header.slots_offset = static_cast<uint32>(offsetof(DataPlaneShm, slots));
        header.slot_stride = static_cast<uint32>(sizeof(ViewSlotShm));
        header.input_cell_offset = static_cast<uint32>(offsetof(ViewSlotShm, input));
        header.input_cell_size = static_cast<uint32>(sizeof(InputCellShm));
        header.projection_cell_offset = static_cast<uint32>(offsetof(ViewSlotShm, projection));
        header.projection_cell_size = static_cast<uint32>(sizeof(ProjectionCellShm));
        header.camera_cell_offset = static_cast<uint32>(offsetof(ViewSlotShm, camera));
        header.camera_cell_size = static_cast<uint32>(sizeof(CameraCellShm));
        header.projected_entry_size = static_cast<uint32>(sizeof(ProjectedEntityShm));
        header.draw_table_offset = static_cast<uint32>(offsetof(DataPlaneShm, draw_layers));
        header.draw_layer_count = DATA_PLANE_DRAW_LAYER_COUNT;
        header.draw_layer_stride = static_cast<uint32>(sizeof(DrawLayerCellShm));
        header.draw_payload_capacity = DATA_PLANE_DRAW_PAYLOAD_CAPACITY;
    }

    Result ensure_data_plane(DataPlaneState& plane, uint16 rpc_port)
    {
        if (plane.shm != nullptr)
            return Result::OK;
        if (plane.create_attempted)
            return Result(false, "Data plane creation already failed this session.");
        plane.create_attempted = true;

        auto error = std::error_code();
        const auto directory = std::filesystem::temp_directory_path(error) / "toybox";
        if (error)
            return Result(false, "Failed to resolve the temp directory for the data plane.");

        std::filesystem::create_directories(directory, error);
        if (error)
            return Result(false, "Failed to create the data plane temp directory.");

        const auto path = directory
            / std::format("dataplane-{}-{}.shm", current_process_id(), rpc_port);
        if (auto result = plane.mapping.create(path.string(), sizeof(DataPlaneShm)); !result)
            return result;

        // A fresh zero-filled file is already every cell's valid idle state; stamping the header
        // last (after the memset-equivalent zero fill) means a reader that races the create never
        // sees a valid magic over garbage lanes.
        plane.shm = static_cast<DataPlaneShm*>(plane.mapping.data());
        initialize_header(*plane.shm);
        std::atomic_thread_fence(std::memory_order_release);

        TBX_TRACE_INFO(
            "StudioBridge: data plane mapped ({} KB at {}).",
            sizeof(DataPlaneShm) / 1024U,
            plane.mapping.path());
        return Result::OK;
    }

    void destroy_data_plane(DataPlaneState& plane)
    {
        if (plane.shm == nullptr)
        {
            plane.create_attempted = false;
            return;
        }

        const auto path = plane.mapping.path();
        plane.shm = nullptr;
        plane.mapping.destroy();

        auto error = std::error_code();
        std::filesystem::remove(path, error);
        if (error)
            TBX_TRACE_WARNING("StudioBridge: failed to delete the data plane file '{}'.", path);

        plane.create_attempted = false;
    }

    bool is_data_plane_available(const DataPlaneState& plane)
    {
        return plane.shm != nullptr;
    }

    tbx::Json describe_data_plane(const DataPlaneState& plane)
    {
        if (plane.shm == nullptr)
            return tbx::Json();

        auto advert = tbx::Json::object();
        advert[Wire::PATH] = plane.mapping.path();
        advert[Wire::LAYOUT_VERSION] = plane.shm->header.layout_version;
        advert[Wire::SIZE] = plane.shm->header.total_size;
        return advert;
    }

    int32 acquire_view_slot(DataPlaneState& plane, const std::string& view_name)
    {
        if (plane.shm == nullptr || view_name.empty())
            return -1;

        for (uint32 index = 0U; index < DATA_PLANE_MAX_VIEWS; ++index)
        {
            auto& slot = plane.shm->slots[index];
            if (slot.active != 0U)
                continue;

            const auto generation = plane.next_generation++;
            if (plane.next_generation == 0U)
                plane.next_generation = 1U;

            // Reset the lanes' bookkeeping so the new view never reads its predecessor's data. The
            // bulk payloads can stay — count/sequence gate what readers consume.
            slot.input.sequence.store(0U, std::memory_order_relaxed);
            slot.input.generation = 0U;
            slot.input.snapshot = InputSnapshotShm();
            slot.projection.sequence.store(0U, std::memory_order_relaxed);
            slot.projection.count = 0U;
            slot.projection.overflow_count = 0U;
            slot.projection.frame_index = 0U;
            slot.camera.sequence.store(0U, std::memory_order_relaxed);
            slot.camera.frame_index = 0U;
            slot.active = 1U;
            slot.generation.store(generation, std::memory_order_release);
            plane.slot_views[index] = view_name;
            plane.drain[index] = SlotDrainState();
            return static_cast<int32>(index);
        }

        return -1;
    }

    static void release_slot_at(DataPlaneState& plane, uint32 index)
    {
        auto& shm_slot = plane.shm->slots[index];
        shm_slot.active = 0U;
        shm_slot.generation.store(0U, std::memory_order_release);
        plane.slot_views[index].clear();
        plane.drain[index] = SlotDrainState();
    }

    void release_view_slot(DataPlaneState& plane, const std::string& view_name)
    {
        if (plane.shm == nullptr || view_name.empty())
            return;

        for (uint32 index = 0U; index < DATA_PLANE_MAX_VIEWS; ++index)
            if (plane.slot_views[index] == view_name)
                release_slot_at(plane, index);
    }

    void release_all_view_slots(DataPlaneState& plane)
    {
        if (plane.shm == nullptr)
            return;

        for (uint32 index = 0U; index < DATA_PLANE_MAX_VIEWS; ++index)
            if (!plane.slot_views[index].empty())
                release_slot_at(plane, index);
    }

    uint32 view_slot_generation(const DataPlaneState& plane, int32 slot)
    {
        if (plane.shm == nullptr || slot < 0 || slot >= static_cast<int32>(DATA_PLANE_MAX_VIEWS))
            return 0U;

        return plane.shm->slots[static_cast<uint32>(slot)].generation.load(
            std::memory_order_acquire);
    }

    // Seqlock read of one input cell. Returns false when the cell holds nothing new: never written,
    // sequence unchanged since the last drain, a stale generation, or a writer that kept colliding
    // (bounded retries — the next frame picks it up).
    static bool read_input_cell(
        const InputCellShm& cell, uint32 expected_generation, SlotDrainState& drain,
        InputSnapshotShm& out_snapshot)
    {
        for (auto attempt = 0; attempt < 8; ++attempt)
        {
            const auto sequence = cell.sequence.load(std::memory_order_acquire);
            if (sequence == 0U || sequence == drain.last_sequence)
                return false;
            if ((sequence & 1U) != 0U)
                continue; // mid-write; retry

            const auto generation = cell.generation;
            std::memcpy(&out_snapshot, &cell.snapshot, sizeof(InputSnapshotShm));
            std::atomic_thread_fence(std::memory_order_acquire);
            if (cell.sequence.load(std::memory_order_relaxed) != sequence)
                continue; // torn read; retry

            if (generation != expected_generation)
                return false; // a stale writer (the slot was reused); ignore it
            drain.last_sequence = sequence;
            return true;
        }

        return false;
    }

    void drain_input_lanes(DataPlaneState& plane, ViewState& views)
    {
        if (plane.shm == nullptr)
            return;

        auto lock = std::lock_guard(views.mutex);
        for (uint32 index = 0U; index < DATA_PLANE_MAX_VIEWS; ++index)
        {
            const auto& view_name = plane.slot_views[index];
            if (view_name.empty())
                continue;

            const auto input_it = views.inputs.find(view_name);
            if (input_it == views.inputs.end())
                continue;

            auto& slot = plane.shm->slots[index];
            auto& drain = plane.drain[index];
            auto snapshot = InputSnapshotShm();
            if (!read_input_cell(
                    slot.input, slot.generation.load(std::memory_order_relaxed), drain, snapshot))
                continue;

            // Same application the view.input RPC fallback performs (apply_view_input), except the
            // deltas come from the cumulative-total diff — so overwritten snapshots lose nothing.
            auto& input = input_it->second;
            input.focused = snapshot.focused != 0U;
            input.buttons = snapshot.buttons;
            input.accumulated_mouse_dx += static_cast<float>(snapshot.total_dx - drain.last_dx);
            input.accumulated_mouse_dy += static_cast<float>(snapshot.total_dy - drain.last_dy);
            input.accumulated_wheel += static_cast<float>(snapshot.total_wheel - drain.last_wheel);
            drain.last_dx = snapshot.total_dx;
            drain.last_dy = snapshot.total_dy;
            drain.last_wheel = snapshot.total_wheel;
            input.cursor_u = snapshot.cursor_u;
            input.cursor_v = snapshot.cursor_v;
            input.move_keys = snapshot.move_keys;
            input.keys.clear();
            const auto key_count = std::min(
                snapshot.key_count, static_cast<uint32>(std::size(snapshot.keys)));
            for (uint32 key = 0U; key < key_count; ++key)
                input.keys.push_back(snapshot.keys[key]);
            input.mouse_x = snapshot.mouse_x;
            input.mouse_y = snapshot.mouse_y;
        }
    }

    // Seqlock write bracket for the engine-owned (single main-thread writer) cells.
    static uint32 begin_cell_write(std::atomic<uint32>& sequence)
    {
        const auto current = sequence.load(std::memory_order_relaxed);
        sequence.store(current + 1U, std::memory_order_relaxed); // odd: write in progress
        std::atomic_thread_fence(std::memory_order_release);
        return current;
    }

    static void end_cell_write(std::atomic<uint32>& sequence, uint32 begun)
    {
        sequence.store(begun + 2U, std::memory_order_release); // even: published
    }

    static void publish_projection_cell(
        ProjectionCellShm& cell,
        const std::vector<tbx::EntityScreenPosition>& positions,
        uint64 frame_index)
    {
        const auto count = std::min(
            static_cast<uint32>(positions.size()), DATA_PLANE_MAX_PROJECTED_ENTITIES);
        const auto begun = begin_cell_write(cell.sequence);
        cell.count = count;
        cell.overflow_count = static_cast<uint32>(positions.size()) - count;
        cell.frame_index = frame_index;
        for (uint32 index = 0U; index < count; ++index)
        {
            const auto& position = positions[index];
            auto& item = cell.items[index];
            item.id = position.id.value;
            item.u = position.u;
            item.v = position.v;
            item.depth = position.depth;
            item.flags = 0U;
        }
        end_cell_write(cell.sequence, begun);
    }

    static void publish_camera_cell(
        CameraCellShm& cell, const ViewStream& view, uint64 frame_index)
    {
        const auto& camera = view.view.camera;
        const auto view_matrix = camera.get_view_matrix(view.view.position, view.view.rotation);
        const auto view_projection =
            camera.get_view_projection_matrix(view.view.position, view.view.rotation);

        const auto begun = begin_cell_write(cell.sequence);
        cell.is_orthographic = camera.is_orthographic() ? 1U : 0U;
        cell.width = static_cast<uint32>(view.texture.size.width);
        cell.height = static_cast<uint32>(view.texture.size.height);
        std::memcpy(cell.view_projection, glm::value_ptr(view_projection), sizeof(cell.view_projection));
        std::memcpy(cell.view, glm::value_ptr(view_matrix), sizeof(cell.view));
        std::memcpy(
            cell.projection, glm::value_ptr(camera.get_projection_matrix()), sizeof(cell.projection));
        cell.position[0] = view.view.position.x;
        cell.position[1] = view.view.position.y;
        cell.position[2] = view.view.position.z;
        cell.rotation[0] = view.view.rotation.x;
        cell.rotation[1] = view.view.rotation.y;
        cell.rotation[2] = view.view.rotation.z;
        cell.rotation[3] = view.view.rotation.w;
        cell.z_near = camera.get_z_near();
        cell.z_far = camera.get_z_far();
        cell.fov_degrees = camera.get_fov();
        cell.aspect = camera.get_aspect();
        cell.frame_index = frame_index;
        end_cell_write(cell.sequence, begun);
    }

    void publish_view_lanes(DataPlaneState& plane, ViewState& views, const EngineServices& services)
    {
        if (plane.shm == nullptr)
            return;

        // The active world resolves through the world manager (its own locking); fetch it before
        // taking the views lock — the same order sync_camera_entities uses.
        const auto active = services.active_world();
        const auto frame_index = ++plane.frame_counter;

        auto lock = std::lock_guard(views.mutex);
        for (const auto& view : views.streams)
        {
            // Skip views without a slot, and views that have settled idle: their camera stopped
            // moving and the world isn't animating, so the last published snapshot still holds.
            if (view->idle_settle_frames == 0U)
                continue;
            const auto slot_it = std::find(
                plane.slot_views.begin(), plane.slot_views.end(), view->name);
            if (slot_it == plane.slot_views.end())
                continue;
            const auto slot_index =
                static_cast<uint32>(std::distance(plane.slot_views.begin(), slot_it));

            const auto world = view->world_id == 0U
                ? active
                : [&views, &view]() -> std::shared_ptr<tbx::World>
                  {
                      const auto it = views.worlds.find(view->world_id);
                      return it != views.worlds.end() ? it->second : nullptr;
                  }();
            if (!world)
                continue;

            auto& slot = plane.shm->slots[slot_index];
            publish_projection_cell(
                slot.projection,
                tbx::project_entities_to_screen(view->view, *world),
                frame_index);
            publish_camera_cell(slot.camera, *view, frame_index);
        }
    }
}
