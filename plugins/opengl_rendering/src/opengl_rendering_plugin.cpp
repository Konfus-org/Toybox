#include "opengl_rendering_plugin.h"
#include "tbx/app/application.h"
#include "tbx/debugging/macros.h"
#include "tbx/graphics/graphics_settings.h"
#include "tbx/messages/observable.h"
#include <functional>
#include <future>
#include <string_view>
#include <vector>

namespace opengl_rendering
{
    static constexpr std::string_view OPENGL_RENDER_LANE_NAME = "render";

    static OpenGlShadowSettings build_shadow_settings(const GraphicsSettings& graphics_settings)
    {
        return OpenGlShadowSettings {
            .shadow_map_resolution = graphics_settings.shadow_map_resolution.value,
            .shadow_render_distance = graphics_settings.shadow_render_distance.value,
            .shadow_softness = graphics_settings.shadow_softness.value,
        };
    }

    void OpenGlRenderingPlugin::on_attach(tbx::IPluginHost& host)
    {
        const auto& graphics_settings = host.get_settings().graphics;
        _shadow_settings = build_shadow_settings(graphics_settings);
        host.get_thread_manager().try_create_lane(OPENGL_RENDER_LANE_NAME);
    }

    void OpenGlRenderingPlugin::on_detach()
    {
        auto window_ids = std::vector<Uuid> {};
        window_ids.reserve(_renderers.size());
        for (const auto& key : _renderers | std::views::keys)
            window_ids.push_back(key);

        for (const auto& window_id : window_ids)
            teardown_renderer(window_id);
    }

    void OpenGlRenderingPlugin::on_update(const tbx::DeltaTime&)
    {
        auto& thread_manager = get_host().get_thread_manager();
        auto render_futures = std::vector<std::future<void>> {};
        render_futures.reserve(_renderers.size());

        for (auto& val : _renderers | std::views::values)
        {
            auto* renderer = val.get();
            if (!renderer)
                continue;

            if (!thread_manager.has_lane(OPENGL_RENDER_LANE_NAME))
            {
                renderer->render();
                continue;
            }

            render_futures.push_back(thread_manager.post_with_future(
                OPENGL_RENDER_LANE_NAME,
                [renderer]()
                {
                    renderer->render();
                }));
        }

        for (auto& render_future : render_futures)
            render_future.get();
    }

    void OpenGlRenderingPlugin::on_recieve_message(tbx::Message& msg)
    {
        if (auto* ready_event = handle_message<WindowContextReadyEvent>(msg))
        {
            auto context = OpenGlContext(get_host().get_message_coordinator(), ready_event->window);
            auto& thread_manager = get_host().get_thread_manager();
            auto renderer = std::unique_ptr<OpenGlRenderer> {};
            if (thread_manager.has_lane(OPENGL_RENDER_LANE_NAME))
            {
                auto entity_registry = std::ref(get_host().get_entity_registry());
                auto asset_manager = std::ref(get_host().get_asset_manager());
                auto shadow_settings = _shadow_settings;
                auto render_resolution = ready_event->size;
                auto create_renderer_future = thread_manager.post_with_future(
                    OPENGL_RENDER_LANE_NAME,
                    [loader = ready_event->get_proc_address,
                     entity_registry,
                     asset_manager,
                     context = std::move(context),
                     shadow_settings,
                     render_resolution]() mutable
                    {
                        auto gl_renderer = std::make_unique<OpenGlRenderer>(
                            loader,
                            entity_registry.get(),
                            asset_manager.get(),
                            std::move(context),
                            shadow_settings);
                        gl_renderer->set_viewport_size(render_resolution);
                        gl_renderer->set_pending_render_resolution(render_resolution);
                        return gl_renderer;
                    });
                renderer = create_renderer_future.get();
            }
            else
            {
                renderer = std::make_unique<OpenGlRenderer>(
                    ready_event->get_proc_address,
                    get_host().get_entity_registry(),
                    get_host().get_asset_manager(),
                    std::move(context),
                    _shadow_settings);
                renderer->set_viewport_size(ready_event->size);
                renderer->set_pending_render_resolution(ready_event->size);
            }

            _renderers[ready_event->window] = std::move(renderer);

            TBX_TRACE_INFO(
                "OpenGL rendering: renderer ready for window {}.",
                to_string(ready_event->window));
            return;
        }

        if (const auto* open_event = handle_property_changed<&Window::is_open>(msg))
        {
            if (!open_event->current && open_event->owner)
                teardown_renderer(open_event->owner->id);
            return;
        }

        if (const auto* size_event = handle_property_changed<&Window::size>(msg))
        {
            if (!size_event->owner)
                return;

            const auto renderer_it = _renderers.find(size_event->owner->id);
            if (renderer_it == _renderers.end())
                return;

            auto* renderer = renderer_it->second.get();
            if (!renderer)
                return;

            if (auto& thread_manager = get_host().get_thread_manager();
                thread_manager.has_lane(OPENGL_RENDER_LANE_NAME))
            {
                auto viewport_size = size_event->current;
                thread_manager
                    .post_with_future(
                        OPENGL_RENDER_LANE_NAME,
                        [renderer, viewport_size]()
                        {
                            renderer->set_viewport_size(viewport_size);
                            renderer->set_pending_render_resolution(viewport_size);
                        })
                    .get();
            }
            else
            {
                renderer->set_viewport_size(size_event->current);
                renderer->set_pending_render_resolution(size_event->current);
            }
            return;
        }

        if (const auto* shadow_resolution_event =
                handle_property_changed<&GraphicsSettings::shadow_map_resolution>(msg))
        {
            _shadow_settings.shadow_map_resolution = shadow_resolution_event->current;
            apply_shadow_settings_to_renderers();
            return;
        }

        if (const auto* shadow_distance_event =
                handle_property_changed<&GraphicsSettings::shadow_render_distance>(msg))
        {
            _shadow_settings.shadow_render_distance = shadow_distance_event->current;
            apply_shadow_settings_to_renderers();
            return;
        }

        if (const auto* shadow_softness_event =
                handle_property_changed<&GraphicsSettings::shadow_softness>(msg))
        {
            _shadow_settings.shadow_softness = shadow_softness_event->current;
            apply_shadow_settings_to_renderers();
            return;
        }
    }

    void OpenGlRenderingPlugin::teardown_renderer(const Uuid& window_id)
    {
        auto renderer_it = _renderers.find(window_id);
        if (renderer_it == _renderers.end())
            return;

        auto renderer = std::move(renderer_it->second);
        _renderers.erase(renderer_it);
        if (!renderer)
            return;

        auto* released_renderer = renderer.release();
        auto& thread_manager = get_host().get_thread_manager();
        if (thread_manager.has_lane(OPENGL_RENDER_LANE_NAME))
        {
            thread_manager
                .post_with_future(
                    OPENGL_RENDER_LANE_NAME,
                    [released_renderer]()
                    {
                        auto renderer_on_render_lane =
                            std::unique_ptr<OpenGlRenderer>(released_renderer);
                    })
                .get();
        }
        else
        {
            auto renderer_on_main_thread = std::unique_ptr<OpenGlRenderer>(released_renderer);
        }
    }

    void OpenGlRenderingPlugin::apply_shadow_settings_to_renderers() const
    {
        auto& thread_manager = get_host().get_thread_manager();
        auto apply_futures = std::vector<std::future<void>> {};
        apply_futures.reserve(_renderers.size());

        for (const auto& val : _renderers | std::views::values)
        {
            auto* renderer = val.get();
            if (!renderer)
                continue;

            if (thread_manager.has_lane(OPENGL_RENDER_LANE_NAME))
            {
                auto shadow_settings = _shadow_settings;
                apply_futures.push_back(thread_manager.post_with_future(
                    OPENGL_RENDER_LANE_NAME,
                    [renderer, shadow_settings]()
                    {
                        renderer->set_shadow_settings(shadow_settings);
                    }));
            }
            else
            {
                renderer->set_shadow_settings(_shadow_settings);
            }
        }

        for (auto& apply_future : apply_futures)
            apply_future.get();
    }
}
