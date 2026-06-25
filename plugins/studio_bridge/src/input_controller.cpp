#include "input_controller.h"
#include "bridge_geometry.h"
#include "tbx/types/components/transform.h"
#include "tbx/types/vectors.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string_view>

namespace tbx::studio_bridge
{
    static std::string_view to_lock_mode_name(tbx::MouseLockMode mode)
    {
        switch (mode)
        {
            case tbx::MouseLockMode::RELATIVE:
                return "relative";
            case tbx::MouseLockMode::INPUT_GRABBED:
                return "grabbed";
            case tbx::MouseLockMode::UNLOCKED:
                return "unlocked";
        }

        return "unlocked";
    }

    InputController::InputController(EngineServices& services, ViewManager& views)
        : _services(services)
        , _views(views)
    {
    }

    void InputController::update_editor_cameras(const tbx::DeltaTime& dt)
    {
        constexpr float LOOK_SENSITIVITY = 0.0045F; // radians per pixel
        constexpr float MOVE_SPEED = 6.0F; // metres per second
        constexpr float WHEEL_DOLLY = 0.6F; // metres per wheel notch
        constexpr float PAN_SENSITIVITY = 0.01F; // metres per pixel
        constexpr float MAX_PITCH_DOT = 0.99F; // stop just short of straight up/down
        constexpr uint32 RIGHT_BUTTON = 0x2U;
        constexpr uint32 MIDDLE_BUTTON = 0x4U;
        const auto world_up = glm::vec3(0.0F, 1.0F, 0.0F);

        const auto seconds = static_cast<float>(dt.seconds);
        auto world = _services.active_world();

        _views.with_views_locked(
            [&](std::vector<std::unique_ptr<ViewStream>>& views, tbx::EntityRegistry& registry)
            {
                for (auto& view : views)
                {
                    // Game views feed the game input system, not a fly camera (handled in
                    // update_game_input, which owns consuming their deltas), so leave them untouched.
                    if (view->kind == ViewKind::Game)
                        continue;

                    // Asset-preview views orbit a target instead of flying free: drag rotates around
                    // the asset, the wheel zooms in/out, and the camera always looks inward. Its world
                    // is the isolated preview world, so the active-world checks below don't apply.
                    if (view->kind == ViewKind::AssetPreview)
                    {
                        auto camera = registry.get(view->camera_id);
                        if (!camera.get_id().is_valid() || !camera.has_component<tbx::Transform>())
                            continue;
                        auto& orbit_transform = camera.get_component<tbx::Transform>();

                        if (view->focused)
                        {
                            constexpr uint32 LEFT_BUTTON = 0x1U;
                            constexpr uint32 RIGHT_BUTTON = 0x2U;
                            constexpr float ORBIT_SENSITIVITY = 0.01F; // radians per pixel
                            constexpr float ZOOM_SENSITIVITY = 0.12F;  // per wheel notch
                            constexpr float MIN_PITCH = -1.5F;         // just short of straight down
                            constexpr float MAX_PITCH = 1.5F;          // just short of straight up
                            constexpr float MIN_DISTANCE = 0.1F;

                            if ((view->buttons & (LEFT_BUTTON | RIGHT_BUTTON)) != 0U)
                            {
                                view->orbit_yaw -= view->accumulated_mouse_dx * ORBIT_SENSITIVITY;
                                view->orbit_pitch += view->accumulated_mouse_dy * ORBIT_SENSITIVITY;
                                view->orbit_pitch =
                                    std::clamp(view->orbit_pitch, MIN_PITCH, MAX_PITCH);
                            }
                            if (view->accumulated_wheel != 0.0F)
                                view->orbit_distance = std::max(
                                    MIN_DISTANCE,
                                    view->orbit_distance
                                        * std::exp(-view->accumulated_wheel * ZOOM_SENSITIVITY));
                        }

                        const auto cos_pitch = std::cos(view->orbit_pitch);
                        const auto offset = glm::vec3(
                            cos_pitch * std::sin(view->orbit_yaw),
                            std::sin(view->orbit_pitch),
                            cos_pitch * std::cos(view->orbit_yaw));
                        const auto target = glm::vec3(
                            view->orbit_target.x, view->orbit_target.y, view->orbit_target.z);
                        const auto position = target + (offset * view->orbit_distance);
                        orbit_transform.position = tbx::Vec3(position.x, position.y, position.z);
                        const auto heading = target - position;
                        if (glm::length(heading) > 0.001F)
                            orbit_transform.rotation = look_rotation(heading, world_up);

                        view->accumulated_mouse_dx = 0.0F;
                        view->accumulated_mouse_dy = 0.0F;
                        view->accumulated_wheel = 0.0F;
                        continue;
                    }

                    if (!view->camera_id.is_valid() || !world)
                    {
                        view->accumulated_mouse_dx = 0.0F;
                        view->accumulated_mouse_dy = 0.0F;
                        view->accumulated_wheel = 0.0F;
                        continue;
                    }

                    auto entity = registry.get(view->camera_id);
                    if (!entity.get_id().is_valid() || !entity.has_component<tbx::Transform>())
                        continue;

                    auto& transform = entity.get_component<tbx::Transform>();

                    // One-time, once the world's geometry has streamed in: aim the camera across it
                    // so the viewport opens on the world rather than wherever the game camera happened
                    // to be authored facing. The aim is flattened to the horizon: a camera spawned
                    // near the world's horizontal centre has the centroid almost straight overhead, so
                    // looking *at* it would crane the view up at the sky and make WASD fly straight up
                    // — opening level keeps navigation intuitive.
                    if (view->needs_orient)
                    {
                        auto focus = glm::vec3(0.0F);
                        if (compute_world_focus(*world, focus))
                        {
                            auto heading = focus - transform.position;
                            heading.y = 0.0F;
                            if (glm::length(heading) < 0.001F)
                            {
                                // Centroid is directly above/below: keep the current heading, also
                                // levelled.
                                heading = transform.rotation * glm::vec3(0.0F, 0.0F, -1.0F);
                                heading.y = 0.0F;
                            }
                            if (glm::length(heading) < 0.001F)
                                heading = glm::vec3(0.0F, 0.0F, -1.0F);

                            transform.rotation = look_rotation(heading, world_up);
                            view->needs_orient = false;
                        }
                    }

                    // Only the focused editor view drives the fly camera; clear pending deltas
                    // otherwise so they do not burst when focus returns.
                    if (!view->focused)
                    {
                        view->accumulated_mouse_dx = 0.0F;
                        view->accumulated_mouse_dy = 0.0F;
                        view->accumulated_wheel = 0.0F;
                        continue;
                    }

                    // Look only while right mouse is held, and rotate the camera's *current*
                    // orientation incrementally (world-up yaw + local-right pitch) so it stays
                    // roll-free and the spawn heading — which faces the world — is preserved until the
                    // user actually looks around.
                    if ((view->buttons & RIGHT_BUTTON) != 0U
                        && (view->accumulated_mouse_dx != 0.0F || view->accumulated_mouse_dy != 0.0F))
                    {
                        auto rotated =
                            glm::angleAxis(-view->accumulated_mouse_dx * LOOK_SENSITIVITY, world_up)
                            * transform.rotation;
                        const auto local_right = glm::normalize(rotated * glm::vec3(1.0F, 0.0F, 0.0F));
                        const auto pitched =
                            glm::angleAxis(-view->accumulated_mouse_dy * LOOK_SENSITIVITY, local_right)
                            * rotated;
                        if (std::abs((pitched * glm::vec3(0.0F, 0.0F, -1.0F)).y) < MAX_PITCH_DOT)
                            rotated = pitched;
                        transform.rotation = glm::normalize(rotated);
                    }

                    const auto rotation = transform.rotation;
                    const auto forward = rotation * glm::vec3(0.0F, 0.0F, -1.0F);
                    const auto right = rotation * glm::vec3(1.0F, 0.0F, 0.0F);

                    auto move = glm::vec3(0.0F);
                    if ((view->move_keys & 0x01U) != 0U)
                        move += forward;
                    if ((view->move_keys & 0x02U) != 0U)
                        move -= forward;
                    if ((view->move_keys & 0x04U) != 0U)
                        move -= right;
                    if ((view->move_keys & 0x08U) != 0U)
                        move += right;
                    if ((view->move_keys & 0x10U) != 0U)
                        move += world_up;
                    if ((view->move_keys & 0x20U) != 0U)
                        move -= world_up;
                    if (glm::dot(move, move) > 0.0F)
                        transform.position += glm::normalize(move) * (MOVE_SPEED * seconds);

                    if (view->accumulated_wheel != 0.0F)
                        transform.position += forward * (view->accumulated_wheel * WHEEL_DOLLY);

                    if ((view->buttons & MIDDLE_BUTTON) != 0U)
                    {
                        transform.position += right * (-view->accumulated_mouse_dx * PAN_SENSITIVITY);
                        transform.position += world_up * (view->accumulated_mouse_dy * PAN_SENSITIVITY);
                    }

                    view->accumulated_mouse_dx = 0.0F;
                    view->accumulated_mouse_dy = 0.0F;
                    view->accumulated_wheel = 0.0F;
                }
            });
    }

    void InputController::update_game_input(bool is_playing)
    {
        auto input_manager = _services.input_manager.lock();
        if (!input_manager)
            return;

        auto keyboard = tbx::KeyboardState();
        auto mouse = tbx::MouseState();
        auto inject = false;

        _views.with_views_locked(
            [&](std::vector<std::unique_ptr<ViewStream>>& views, tbx::EntityRegistry&)
            {
                for (auto& view : views)
                {
                    if (view->kind != ViewKind::Game)
                        continue;

                    // The first focused game view drives the game while playing; build its input
                    // state before consuming this view's accumulated deltas.
                    if (is_playing && view->focused && !inject)
                    {
                        for (const auto key : view->keys)
                            keyboard.pressed_keys.insert(key);

                        // Studio button bits (0 left, 1 right, 2 middle) → SDL ids (1 left, 2 middle,
                        // 3 right).
                        if ((view->buttons & 0x1U) != 0U)
                            mouse.pressed_buttons.insert(1);
                        if ((view->buttons & 0x4U) != 0U)
                            mouse.pressed_buttons.insert(2);
                        if ((view->buttons & 0x2U) != 0U)
                            mouse.pressed_buttons.insert(3);

                        mouse.position = tbx::Vec2(view->mouse_x, view->mouse_y);
                        mouse.delta = tbx::Vec2(view->accumulated_mouse_dx, view->accumulated_mouse_dy);
                        mouse.wheel_delta = view->accumulated_wheel;
                        inject = true;
                    }

                    // Always consume deltas so they never burst when play/focus resumes.
                    view->accumulated_mouse_dx = 0.0F;
                    view->accumulated_mouse_dy = 0.0F;
                    view->accumulated_wheel = 0.0F;
                }
            });

        input_manager->set_input_injection_enabled(inject);
        if (inject)
        {
            input_manager->set_injected_keyboard(keyboard);
            input_manager->set_injected_mouse(mouse);
        }
    }

    void InputController::report_mouse_lock(bool is_playing)
    {
        const auto host = _services.rpc_host.lock();
        if (!host || !host->has_client())
            return;

        auto input_manager = _services.input_manager.lock();
        // Only a playing game drives the lock mode; outside play the editor cursor is always free.
        const auto mode = (is_playing && input_manager) ? input_manager->get_mouse_lock_mode()
                                                        : tbx::MouseLockMode::UNLOCKED;
        if (mode == _last_reported_lock)
            return;

        _last_reported_lock = mode;
        auto params = tbx::Json::object();
        params["mode"] = to_lock_mode_name(mode);
        host->send_notification("input.mouseLock", params);
    }
}
