#include "input_controller.h"
#include "bridge_utils.h"
#include "view_input.h"
#include "wire.h"
#include "tbx/systems/input/input_manager.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/vectors.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

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
        auto world = _services.get().active_world();

        _views.get().with_views_locked(
            [&](std::vector<std::unique_ptr<ViewStream>>& views,
                std::unordered_map<std::string, ViewInput>& inputs)
            {
                for (auto& view_ptr : views)
                {
                    // Game views feed the game input system, not a fly camera (handled in
                    // update_game_input, which owns consuming their deltas), so leave them untouched.
                    if (dynamic_cast<GameViewStream*>(view_ptr.get()) != nullptr)
                        continue;

                    auto& input = inputs[view_ptr->name];

                    // Asset-preview views orbit a target instead of flying free: drag rotates around
                    // the asset, the wheel zooms in/out, and the camera always looks inward.
                    if (auto* preview = dynamic_cast<AssetPreviewViewStream*>(view_ptr.get()))
                    {
                        if (input.focused)
                        {
                            constexpr uint32 LEFT_BUTTON = 0x1U;
                            constexpr uint32 ORBIT_RIGHT_BUTTON = 0x2U;
                            constexpr float ORBIT_SENSITIVITY = 0.01F; // radians per pixel
                            constexpr float ZOOM_SENSITIVITY = 0.12F;  // per wheel notch
                            constexpr float MIN_PITCH = -1.5F;         // just short of straight down
                            constexpr float MAX_PITCH = 1.5F;          // just short of straight up
                            constexpr float MIN_DISTANCE = 0.1F;

                            if ((input.buttons & (LEFT_BUTTON | ORBIT_RIGHT_BUTTON)) != 0U)
                            {
                                preview->orbit_yaw -=
                                    input.accumulated_mouse_dx * ORBIT_SENSITIVITY;
                                preview->orbit_pitch +=
                                    input.accumulated_mouse_dy * ORBIT_SENSITIVITY;
                                preview->orbit_pitch =
                                    std::clamp(preview->orbit_pitch, MIN_PITCH, MAX_PITCH);
                            }
                            if (input.accumulated_wheel != 0.0F)
                                preview->orbit_distance = std::max(
                                    MIN_DISTANCE,
                                    preview->orbit_distance
                                        * std::exp(-input.accumulated_wheel * ZOOM_SENSITIVITY));
                        }

                        // Turntable: spin the camera while no button is held (a drag takes over, then it
                        // resumes), so previews show the asset rotating on their own.
                        constexpr float AUTO_ORBIT_SPEED = 0.6F; // radians per second
                        if (preview->auto_orbit && input.buttons == 0U)
                            preview->orbit_yaw -= AUTO_ORBIT_SPEED * seconds;

                        const auto cos_pitch = std::cos(preview->orbit_pitch);
                        const auto offset = glm::vec3(
                            cos_pitch * std::sin(preview->orbit_yaw),
                            std::sin(preview->orbit_pitch),
                            cos_pitch * std::cos(preview->orbit_yaw));
                        const auto target = glm::vec3(
                            preview->orbit_target.x, preview->orbit_target.y, preview->orbit_target.z);
                        const auto position = target + (offset * preview->orbit_distance);
                        preview->view.position = tbx::Vec3(position.x, position.y, position.z);
                        const auto heading = target - position;
                        if (glm::length(heading) > 0.001F)
                            preview->view.rotation = tbx::look_rotation(heading, world_up);

                        input.accumulated_mouse_dx = 0.0F;
                        input.accumulated_mouse_dy = 0.0F;
                        input.accumulated_wheel = 0.0F;
                        continue;
                    }

                    auto* editor = dynamic_cast<EditorViewStream*>(view_ptr.get());
                    if (editor == nullptr)
                        continue;
                    if (!world)
                    {
                        input.accumulated_mouse_dx = 0.0F;
                        input.accumulated_mouse_dy = 0.0F;
                        input.accumulated_wheel = 0.0F;
                        continue;
                    }

                    auto& camera = editor->view;

                    // One-time, once the world's geometry has streamed in: aim the camera across it so
                    // the viewport opens on the world rather than wherever the game camera happened to be
                    // authored facing. The aim is flattened to the horizon so opening WASD navigation
                    // stays level (a centroid almost overhead would crane the view up at the sky).
                    if (editor->needs_orient)
                    {
                        auto focus = glm::vec3(0.0F);
                        if (compute_world_focus(*world, focus))
                        {
                            auto heading = focus - glm::vec3(camera.position);
                            heading.y = 0.0F;
                            if (glm::length(heading) < 0.001F)
                            {
                                heading = camera.rotation * glm::vec3(0.0F, 0.0F, -1.0F);
                                heading.y = 0.0F;
                            }
                            if (glm::length(heading) < 0.001F)
                                heading = glm::vec3(0.0F, 0.0F, -1.0F);

                            camera.rotation = tbx::look_rotation(heading, world_up);
                            editor->needs_orient = false;
                        }
                    }

                    // Only the focused editor view drives the fly camera; clear pending deltas otherwise
                    // so they do not burst when focus returns.
                    if (!input.focused)
                    {
                        input.accumulated_mouse_dx = 0.0F;
                        input.accumulated_mouse_dy = 0.0F;
                        input.accumulated_wheel = 0.0F;
                        continue;
                    }

                    // Look only while right mouse is held, and rotate the camera's *current* orientation
                    // incrementally (world-up yaw + local-right pitch) so it stays roll-free and the
                    // spawn heading is preserved until the user actually looks around.
                    if ((input.buttons & RIGHT_BUTTON) != 0U
                        && (input.accumulated_mouse_dx != 0.0F
                            || input.accumulated_mouse_dy != 0.0F))
                    {
                        auto rotated =
                            glm::angleAxis(-input.accumulated_mouse_dx * LOOK_SENSITIVITY, world_up)
                            * camera.rotation;
                        const auto local_right = glm::normalize(rotated * glm::vec3(1.0F, 0.0F, 0.0F));
                        const auto pitched =
                            glm::angleAxis(-input.accumulated_mouse_dy * LOOK_SENSITIVITY, local_right)
                            * rotated;
                        if (std::abs((pitched * glm::vec3(0.0F, 0.0F, -1.0F)).y) < MAX_PITCH_DOT)
                            rotated = pitched;
                        camera.rotation = glm::normalize(rotated);
                    }

                    const auto rotation = camera.rotation;
                    const auto forward = rotation * glm::vec3(0.0F, 0.0F, -1.0F);
                    const auto right = rotation * glm::vec3(1.0F, 0.0F, 0.0F);

                    auto move = glm::vec3(0.0F);
                    if ((input.move_keys & 0x01U) != 0U)
                        move += forward;
                    if ((input.move_keys & 0x02U) != 0U)
                        move -= forward;
                    if ((input.move_keys & 0x04U) != 0U)
                        move -= right;
                    if ((input.move_keys & 0x08U) != 0U)
                        move += right;
                    if ((input.move_keys & 0x10U) != 0U)
                        move += world_up;
                    if ((input.move_keys & 0x20U) != 0U)
                        move -= world_up;
                    if (glm::dot(move, move) > 0.0F)
                        camera.position += glm::normalize(move) * (MOVE_SPEED * seconds);

                    if (input.accumulated_wheel != 0.0F)
                        camera.position += forward * (input.accumulated_wheel * WHEEL_DOLLY);

                    if ((input.buttons & MIDDLE_BUTTON) != 0U)
                    {
                        camera.position +=
                            right * (-input.accumulated_mouse_dx * PAN_SENSITIVITY);
                        camera.position +=
                            world_up * (input.accumulated_mouse_dy * PAN_SENSITIVITY);
                    }

                    input.accumulated_mouse_dx = 0.0F;
                    input.accumulated_mouse_dy = 0.0F;
                    input.accumulated_wheel = 0.0F;
                }
            });
    }

    void InputController::update_game_input(bool is_playing)
    {
        auto input_manager = _services.get().input_manager.lock();
        if (!input_manager)
            return;

        auto external = tbx::ExternalInput();

        _views.get().with_views_locked(
            [&](std::vector<std::unique_ptr<ViewStream>>& views,
                std::unordered_map<std::string, ViewInput>& inputs)
            {
                for (auto& view_ptr : views)
                {
                    auto* game = dynamic_cast<GameViewStream*>(view_ptr.get());
                    if (game == nullptr)
                        continue;

                    auto& input = inputs[game->name];

                    // The first focused game view drives the game while playing; build its input state
                    // before consuming this view's accumulated deltas.
                    if (is_playing && input.focused && !external.enabled)
                    {
                        for (const auto key : input.keys)
                            external.keyboard.pressed_keys.insert(key);

                        // Studio button bits (0 left, 1 right, 2 middle) → SDL ids (1 left, 2 middle,
                        // 3 right).
                        if ((input.buttons & 0x1U) != 0U)
                            external.mouse.pressed_buttons.insert(1);
                        if ((input.buttons & 0x4U) != 0U)
                            external.mouse.pressed_buttons.insert(2);
                        if ((input.buttons & 0x2U) != 0U)
                            external.mouse.pressed_buttons.insert(3);

                        external.mouse.position = tbx::Vec2(input.mouse_x, input.mouse_y);
                        external.mouse.delta =
                            tbx::Vec2(input.accumulated_mouse_dx, input.accumulated_mouse_dy);
                        external.mouse.wheel_delta = input.accumulated_wheel;
                        external.enabled = true;
                    }

                    // Always consume deltas so they never burst when play/focus resumes.
                    input.accumulated_mouse_dx = 0.0F;
                    input.accumulated_mouse_dy = 0.0F;
                    input.accumulated_wheel = 0.0F;
                }
            });

        // A disabled ExternalInput reverts the engine to the physical device.
        input_manager->set_external_input(external);
    }

    void InputController::report_mouse_lock(bool is_playing)
    {
        const auto host = _services.get().rpc_host.lock();
        if (!host || !host->has_client())
            return;

        auto input_manager = _services.get().input_manager.lock();
        // Only a playing game drives the lock mode; outside play the editor cursor is always free.
        const auto mode = (is_playing && input_manager) ? input_manager->get_mouse_lock_mode()
                                                        : tbx::MouseLockMode::UNLOCKED;
        if (mode == _last_reported_lock)
            return;

        _last_reported_lock = mode;
        auto params = tbx::Json::object();
        params[Wire::MODE] = to_lock_mode_name(mode);
        host->send_notification(Wire::INPUT_MOUSE_LOCK, params);
    }
}
