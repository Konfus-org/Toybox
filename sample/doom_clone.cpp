#include "tbx/app.h"
#include "tbx/core/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/ui/ui.h"
#include <cmath>
#include <cstring>
#include <optional>

// The doom clone, fully data-driven. The App declares the level and the HUD; the level chain
// pulls in everything else: rooms, the player, the enemy (with its chase.luau brain), the floor
// material, the monkey statue from the engine resources, and the gunshot sound. This file is
// only the player controller and the selftest choreography.

int main(int argc, char** argv)
{
    using namespace tbx;
    bool selftest = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--selftest") == 0)
            selftest = true;

    auto app = App {
        .title = "Toybox Doom",
        .asset_root = SAMPLE_ASSETS_PATH,
        .sandbox = "levels/arena.box",
        .ui = "ui/hud.rml"};
    float yaw = 0.0f;
    float pitch = 0.0f;
    int kills = 0;
    Toy player = {};
    auto shot = std::optional<KitInstance> {};
    bool streamed_room_seen = false;
    size hub_toy_count = 0;

    while (run(app))
    {
        auto& sandbox = get_sandbox();
        if (app.frame == 1)
            hub_toy_count = sandbox.get_toy_count();
        if (!player.is_alive())
        {
            // The player is a kit too (kits/player.kit, an ALWAYS entry of the level).
            const auto found = sandbox.find("Player");
            if (!found)
            {
                log_error("levels/arena.box did not spawn a Player");
                return 1;
            }
            player = *found;
        }

        // Mouse look + WASD on the ground plane.
        auto& transform = player.get_block<Transform>();
        yaw -= input::get_mouse_delta().x * 0.003f;
        pitch = std::clamp(pitch - input::get_mouse_delta().y * 0.003f, -1.4f, 1.4f);
        transform.rotation = math::angle_axis(yaw, Vec3(0.0f, 1.0f, 0.0f))
            * math::angle_axis(pitch, Vec3(1.0f, 0.0f, 0.0f));
        const Vec3 forward = transform.rotation * Vec3(0.0f, 0.0f, -1.0f);
        const Vec3 flat_forward = math::normalize(Vec3(forward.x, 0.0f, forward.z));
        const Vec3 right = transform.rotation * Vec3(1.0f, 0.0f, 0.0f);
        const float speed = 6.0f * app.delta_time;
        if (input::is_down(Key::W))
            transform.position += flat_forward * speed;
        if (input::is_down(Key::S))
            transform.position -= flat_forward * speed;
        if (input::is_down(Key::D))
            transform.position += right * speed;
        if (input::is_down(Key::A))
            transform.position -= right * speed;
        if (input::is_pressed(Key::ESCAPE))
            quit();

        // Selftest choreography: hold still, fire at the hub enemy walking into the
        // crosshair, then sprint toward the far room so streaming proves itself.
        bool fire = input::is_mouse_pressed(MouseButton::LEFT);
        if (selftest)
        {
            if (app.frame == 120)
                fire = true;
            if (app.frame > 130)
                transform.position += Vec3(0.0f, 0.0f, -0.5f);
            if (sandbox.get_toy_count() > hub_toy_count)
                streamed_room_seen = true;
            if (app.frame >= 260)
                quit();
        }

        if (fire)
        {
            const auto hit = physics::raycast(transform.position, forward, 200.0f);
            if (hit)
            {
                auto target = Toy(sandbox, hit->toy);
                if (target.is_alive() && target.has_sticker("enemy"))
                {
                    sandbox.despawn(target);
                    ++kills;
                    ui::set_inline_style(
                        get_ui_document(),
                        "kills",
                        std::format("width: {}px;", kills * 40));
                }
                // The gunshot is a kit: a speaker toy with the blip clip, placed at the hit.
                if (shot)
                    sandbox.despawn(*shot);
                shot = std::nullopt;
                const auto shot_kit = get_assets().load_now<Json>("kits/shot.kit");
                const auto body = shot_kit ? get_assets().get(*shot_kit) : std::nullopt;
                if (body)
                    if (const auto spawned = sandbox.spawn(body->get(), hit->position))
                        shot = *spawned;
            }
        }

        sandbox.stream(transform.position);

        gpu::begin_frame({.clear = Color {.r = 0.05f, .g = 0.05f, .b = 0.08f}});
        gpu::render(sandbox, get_assets());
        ui::render();
    }

    if (selftest)
    {
        const bool passed = kills >= 1 && streamed_room_seen;
        log_info(
            "doom selftest: kills={} streamed_room={} -> {}",
            kills,
            streamed_room_seen,
            passed ? "PASSED" : "FAILED");
        return passed ? 0 : 1;
    }
    return 0;
}
