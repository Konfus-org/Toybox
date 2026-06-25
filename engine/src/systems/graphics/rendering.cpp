#include "tbx/systems/graphics/rendering.h"
#include "tbx/interfaces/message_dispatcher.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/systems/graphics/gizmos.h"
#include <cstdint>
#include <fstream>
#include <tuple>
#include <vector>

namespace tbx
{
    constexpr auto RENDER_LANE_NAME = std::string_view("render");

    // Writes BGRA, top-down pixels (the layout IGraphicsBackend::read_back_buffer delivers) as a
    // 32-bit BMP. Used by capture_screenshot so a real rendered frame can be inspected without a
    // window-capture step (GDI/PrintWindow return black for hardware GL surfaces).
    static bool write_bgra_bmp(
        const std::filesystem::path& path,
        uint32 width,
        uint32 height,
        const std::vector<uint8>& bgra_top_down)
    {
        if (width == 0U || height == 0U
            || bgra_top_down.size() < static_cast<size>(width) * height * 4U)
            return false;

        const uint32 pixel_bytes = width * height * 4U;
        const uint32 file_size = 54U + pixel_bytes;
        auto put_u32 = [](uint8* out, uint32 value)
        {
            out[0] = static_cast<uint8>(value & 0xFFU);
            out[1] = static_cast<uint8>((value >> 8U) & 0xFFU);
            out[2] = static_cast<uint8>((value >> 16U) & 0xFFU);
            out[3] = static_cast<uint8>((value >> 24U) & 0xFFU);
        };

        uint8 header[54] = {};
        header[0] = 'B';
        header[1] = 'M';
        put_u32(header + 2, file_size);
        put_u32(header + 10, 54U); // pixel data offset
        put_u32(header + 14, 40U); // DIB header size
        put_u32(header + 18, width);
        // Negative height marks the rows as top-down, matching the readback's delivered order.
        put_u32(header + 22, static_cast<uint32>(-static_cast<int32>(height)));
        header[26] = 1U; // planes
        header[28] = 32U; // bits per pixel
        put_u32(header + 34, pixel_bytes);

        auto stream = std::ofstream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            return false;
        stream.write(reinterpret_cast<const char*>(header), sizeof(header));
        stream.write(reinterpret_cast<const char*>(bgra_top_down.data()), pixel_bytes);
        return stream.good();
    }

    // A target untouched for this many dispatches is treated as a closed view and its lane dropped.
    // Mirrors the OpenGL backend's readback-ring eviction so the two per-target maps age in step.
    constexpr auto RENDER_LANE_STALE_TOUCHES = uint64(256);

    Rendering::Rendering(
        std::weak_ptr<IGraphicsBackend> backend,
        std::weak_ptr<AssetManager> asset_manager,
        std::weak_ptr<ThreadManager> thread_manager,
        std::weak_ptr<IWindowManager> window_manager,
        std::weak_ptr<WorldManager> world_manager,
        std::weak_ptr<IMessageCoordinator> message_coordinator,
        std::weak_ptr<Gizmos> gizmos,
        Handle)
        : _thread_manager(std::move(thread_manager))
        , _message_coordinator(message_coordinator)
        , _backend(std::move(backend))
        , _window_manager(window_manager)
        , _gizmos(std::move(gizmos))
        , _pipeline(
              _backend,
              std::move(asset_manager),
              std::move(window_manager),
              std::move(world_manager))
    {
        if (auto coordinator = _message_coordinator.lock())
        {
            _asset_reload_handler = coordinator->register_handler(
                [this](Message& message)
                {
                    if (const auto reloaded = handle_message<AssetReloadedEvent>(message))
                        on_asset_reloaded(reloaded->get());
                });
        }

        auto thread_manager_service = _thread_manager.lock();
        if (!thread_manager_service)
        {
            TBX_TRACE_ERROR(
                "Rendering initialization failed because thread manager is unavailable.");
            return;
        }

        if (!thread_manager_service->has_lane(RENDER_LANE_NAME)
            && !thread_manager_service->try_create_lane(RENDER_LANE_NAME))
        {
            TBX_TRACE_ERROR("Rendering initialization failed because render lane creation failed.");
            return;
        }
    }

    Rendering::~Rendering() noexcept
    {
        TBX_TRY_CATCH_ASSERT(
            {
                wait_for_render_frame();
                if (auto coordinator = _message_coordinator.lock())
                    coordinator->deregister_handler(_asset_reload_handler);

                if (auto thread_manager = _thread_manager.lock())
                {
                    if (auto backend = _backend.lock())
                    {
                        auto idle_future = thread_manager->post_with_future(
                            std::string(RENDER_LANE_NAME),
                            [backend]()
                            {
                                backend->wait_for_idle();
                            });
                        idle_future.get();
                    }

                    thread_manager->stop_lane(std::string(RENDER_LANE_NAME));
                }
            },
            "Toybox renderer shutdown failed.");
    }

    void Rendering::render(
        const DeltaTime& delta_time,
        const GraphicsSettings& settings,
        const CameraView& camera_view,
        const RenderTarget& output_target,
        const std::vector<PostProcessingEffect>& extra_post_effects,
        const std::shared_ptr<World>& world_override)
    {
        auto thread_manager = _thread_manager.lock();
        if (!thread_manager || !thread_manager->has_lane(RENDER_LANE_NAME))
        {
            TBX_TRACE_ERROR("Toybox renderer render lane is unavailable.");
            return;
        }

        // The main window is either the presentation target or the context host for texture
        // targets; without it there is nothing to render with.
        const auto window_manager = _window_manager.lock();
        if (!window_manager || !window_manager->has_main_window())
            return;

        // Resolve the target on this (main) thread: the window manager is not thread-safe, so
        // the render lane must receive everything it needs by value.
        auto resolved_target = output_target;
        const auto target_window = Window(output_target);
        if (window_manager->has(target_window))
        {
            resolved_target.native_handle = window_manager->get_native_handle(target_window);
            resolved_target.size = window_manager->get_size(target_window);
        }

        TBX_TRY_CATCH_ASSERT(
            {
                auto gizmos = _gizmos.lock();
                // Everything the render lane needs is captured by value (the lane runs later, off this
                // thread) — including the caller's extra post effects, copied so their source can change.
                auto future = thread_manager->post_with_future(
                    RENDER_LANE_NAME,
                    [this, delta_time, settings, camera_view, resolved_target, gizmos, extra_post_effects, world_override]()
                    {
                        render_frame(
                            delta_time,
                            settings,
                            camera_view,
                            resolved_target,
                            gizmos,
                            extra_post_effects,
                            world_override);
                    });

                // Each target owns its own lane so its completion is tracked independently; all
                // work still funnels to the single render lane, so submission order is preserved.
                auto guard = std::lock_guard(_render_lanes_mutex);
                auto& lane = _render_lanes[resolved_target.id.value];
                lane.frame = std::move(future);
                lane.last_touch = ++_lane_touch_counter;
                evict_stale_lanes();
            },
            "Toybox renderer dispatch failed");
    }

    void Rendering::set_pre_present_callback(
        std::function<
            void(IGraphicsBackend& backend, const RenderTarget& output_target, const Size& backbuffer_size)>
            callback)
    {
        _pipeline.set_pre_present_callback(std::move(callback));
    }

    void Rendering::capture_screenshot(
        std::filesystem::path path,
        std::function<void(bool succeeded)> on_complete)
    {
        // Warm-up frames let the world finish its synchronous first-frame asset load before the
        // first capture attempt, so the screenshot isn't of an empty scene. The shared state lets
        // the render-lane callback count attempts across frames and fire on_complete exactly once;
        // the callback never clears itself (the pipeline holds its mutex while invoking it), so a
        // 'finished' flag makes every later invocation a no-op instead.
        struct CaptureState
        {
            std::filesystem::path path = {};
            std::function<void(bool)> on_complete = {};
            int attempts = 0;
            bool finished = false;
        };
        auto state = std::make_shared<CaptureState>(
            CaptureState {.path = std::move(path), .on_complete = std::move(on_complete)});

        set_pre_present_callback(
            [state](IGraphicsBackend& backend, const RenderTarget&, const Size& backbuffer_size)
            {
                constexpr int WARMUP_FRAMES = 8;
                constexpr int MAX_ATTEMPTS = 240;
                if (state->finished)
                    return;

                const int attempt = state->attempts++;
                if (attempt < WARMUP_FRAMES)
                    return;

                auto pixels = std::vector<uint8>();
                if (backend.read_back_buffer(backbuffer_size, pixels) && !pixels.empty())
                {
                    const bool wrote = write_bgra_bmp(
                        state->path, backbuffer_size.width, backbuffer_size.height, pixels);
                    if (wrote)
                        TBX_TRACE_INFO("Saved screenshot to '{}'.", state->path.string());
                    else
                        TBX_TRACE_ERROR("Failed to write screenshot '{}'.", state->path.string());

                    state->finished = true;
                    if (state->on_complete)
                        state->on_complete(wrote);
                }
                else if (attempt >= MAX_ATTEMPTS)
                {
                    TBX_TRACE_ERROR("Screenshot readback never became ready; giving up.");
                    state->finished = true;
                    if (state->on_complete)
                        state->on_complete(false);
                }
            });
    }

    void Rendering::wait_for_pending_frame() noexcept
    {
        wait_for_render_frame();
    }

    void Rendering::on_asset_reloaded(const AssetReloadedEvent& event)
    {
        if (!event.succeeded || !event.affected_asset.id.is_valid())
            return;

        const auto thread_manager = _thread_manager.lock();
        const auto backend = _backend.lock();
        if (!thread_manager || !backend || !thread_manager->has_lane(RENDER_LANE_NAME))
            return;

        std::ignore = thread_manager->post_with_future(
            std::string(RENDER_LANE_NAME),
            [this, backend]()
            {
                if (!backend)
                    return;

                _pipeline.reload();
            });
    }

    void Rendering::render_frame(
        const DeltaTime& delta_time,
        const GraphicsSettings& settings,
        const CameraView& camera_view,
        const RenderTarget& output_target,
        std::shared_ptr<Gizmos> gizmos,
        const std::vector<PostProcessingEffect>& extra_post_effects,
        std::shared_ptr<World> world_override)
    {
        const auto backend = _backend.lock();
        if (!backend)
        {
            TBX_TRACE_ERROR_ONCE("Toybox renderer graphics backend is unavailable.");
            return;
        }

        const auto vsync_mode = settings.vsync_enabled;
        if (backend->get_vsync() != vsync_mode)
        {
            const auto vsync_result = backend->set_vsync(vsync_mode);
            if (!vsync_result)
            {
                TBX_TRACE_ERROR_ONCE(
                    "Toybox renderer failed to apply vsync setting. {}",
                    vsync_result.get_report());
            }
        }

        const auto result = _pipeline.execute(
            settings, delta_time, camera_view, output_target, gizmos.get(), extra_post_effects,
            world_override.get());
        if (!result)
        {
            TBX_TRACE_ERROR("Toybox rendering pipeline execution failed. {}", result.get_report());
        }
    }

    void Rendering::wait_for_render_frame() noexcept
    {
        auto guard = std::lock_guard(_render_lanes_mutex);
        for (auto& [key, lane] : _render_lanes)
        {
            if (!lane.frame.valid())
                continue;

            // Wait each lane independently so one target's failure cannot stop draining the rest.
            TBX_TRY_CATCH_ASSERT(lane.frame.get();, "Toybox renderer frame completion failed.");
        }
    }

    void Rendering::evict_stale_lanes()
    {
        // Stopped views stop calling render(), so their key stops advancing the touch counter.
        // Drop any lane that has fallen far enough behind; by then its last frame has completed.
        for (auto it = _render_lanes.begin(); it != _render_lanes.end();)
        {
            if (_lane_touch_counter - it->second.last_touch > RENDER_LANE_STALE_TOUCHES)
                it = _render_lanes.erase(it);
            else
                ++it;
        }
    }
}
