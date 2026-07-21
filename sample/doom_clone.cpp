#include "tbx/app.h"
#include "tbx/audio/audio.h"
#include "tbx/core/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/ui/ui.h"
#include <cmath>
#include <cstring>

// The doom clone, fully data-driven: the level is levels/arena.box, rooms and the enemy are
// .kit prefabs, the enemy brain is scripts/chase.luau, the floor material references an
// engine-resources texture, the monkey statue is resources/Models/Monkey.fbx, the HUD is
// ui/hud.rml, and the gunshot is sounds/blip.wav. Nothing here is embedded content — this file
// is only the player controller and the selftest choreography.

/// @brief
/// Purpose: The startup manifest: touching each asset once registers its identity so kit files
/// can reference everything by handle.
struct LoadedContent
{
    tbx::AssetHandle<tbx::AudioClip> blip = {};
    uint64 hud_document = 0;
    bool is_ready = false;
};

static LoadedContent load_content()
{
    using namespace tbx;
    auto content = LoadedContent {};
    auto& assets = get_assets();

    const auto chase = assets.load_now<ScriptSource>("scripts/chase.luau");
    const auto monkey = assets.load_now<Model>("Models/Monkey.fbx"); // engine resources root
    const auto floor = assets.load_now<Material>("materials/floor.mat");
    const auto blip = assets.load_now<AudioClip>("sounds/blip.wav");
    const auto hud = assets.load_now<UiDocument>("ui/hud.rml");
    for (const auto* error : {
             chase ? nullptr : &chase.error(),
             monkey ? nullptr : &monkey.error(),
             floor ? nullptr : &floor.error(),
             blip ? nullptr : &blip.error(),
             hud ? nullptr : &hud.error()})
        if (error)
        {
            log_error("content: {}", *error);
            return content;
        }

    content.blip = *blip;
    if (const auto document = ui::load_document(assets.get(*hud)->get().text))
        content.hud_document = *document;

    const auto level = assets.load_now<Json>("levels/arena.box");
    if (!level)
    {
        log_error("level: {}", level.error());
        return content;
    }
    const auto opened = get_sandbox().open(
        {.kits = assets.get(*level)->get(), .resolver = assets.make_kit_resolver()});
    if (!opened)
    {
        log_error("level open: {}", opened.error());
        return content;
    }
    content.is_ready = true;
    return content;
}

int main(int argc, char** argv)
{
    using namespace tbx;
    bool selftest = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--selftest") == 0)
            selftest = true;

    auto app = App {.title = "Toybox Doom", .asset_root = SAMPLE_ASSETS_PATH};
    auto content = LoadedContent {};
    float yaw = 0.0f;
    float pitch = 0.0f;
    int kills = 0;
    Toy player = {};
    Toy shot_speaker = {};
    bool streamed_room_seen = false;
    size hub_toy_count = 0;

    while (run(app))
    {
        auto& sandbox = get_sandbox();

        if (app.frame == 1)
        {
            content = load_content();
            if (!content.is_ready)
                return 1;
            player = sandbox.spawn("Player")
                         .with(Transform {.position = Vec3(0.0f, 1.2f, 6.0f)})
                         .with(Camera {})
                         .with(AudioListener {});
            hub_toy_count = sandbox.get_toy_count();
        }
        if (!player.is_alive())
            continue;

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
                        content.hud_document, "kills",
                        std::format(
                            "position: absolute; left: 16px; top: 16px; width: {}px; "
                            "height: 14px; background-color: #ff3333;",
                            kills * 40));
                }
                if (shot_speaker.is_alive())
                    sandbox.despawn(shot_speaker);
                shot_speaker = sandbox.spawn("Shot")
                                   .with(Transform {.position = hit->position})
                                   .with(AudioSource {.clip = content.blip, .is_playing = true});
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
