#include "tbx/app.h"
#include "tbx/assets/builtin.h"
#include "tbx/audio/audio.h"
#include "tbx/core/log.h"
#include "tbx/files/files.h"
#include "tbx/gfx/gpu.h"
#include "tbx/physics/physics.h"
#include "tbx/platform/input.h"
#include "tbx/serialization/serialization.h"
#include "tbx/ui/ui.h"
#include <cmath>
#include <cstring>
#include <map>

// The doom clone: rooms as streamed kits, enemies as Luau-scripted prefab kits, hitscan via
// physics raycasts, spatial blips through the audio stack, and an RmlUi HUD — the whole engine
// in one small game. Run with --selftest for the scripted proof.

static constexpr const char* CHASE_SCRIPT = R"(
function update(toy, delta_time)
    local player = tbx.sandbox.find("Player")
    if player == nil then return end
    local target = player:get("Transform").position
    local transform = toy:get("Transform")
    local at = transform.position
    local dx = target.x - at.x
    local dz = target.z - at.z
    local distance = math.sqrt(dx * dx + dz * dz)
    if distance > 1.5 then
        local step = 2.0 * delta_time / distance
        transform.position = { x = at.x + dx * step, y = at.y, z = at.z + dz * step }
    end
end
)";

static constexpr const char* HUD_RML = R"(<rml>
<head><style>
body { width: 100%; height: 100%; }
#crosshair_h { position: absolute; left: 50%; top: 50%; margin-left: -12px; margin-top: -1px;
               width: 24px; height: 2px; background-color: #ffffffcc; }
#crosshair_v { position: absolute; left: 50%; top: 50%; margin-left: -1px; margin-top: -12px;
               width: 2px; height: 24px; background-color: #ffffffcc; }
#kills { position: absolute; left: 16px; top: 16px; width: 0px; height: 14px;
         background-color: #ff3333; }
#health { position: absolute; left: 16px; top: 36px; width: 200px; height: 14px;
          background-color: #33cc33; }
</style></head>
<body>
    <div id="crosshair_h"/>
    <div id="crosshair_v"/>
    <div id="kills"/>
    <div id="health"/>
</body>
</rml>)";

/// @brief
/// Purpose: Writes a short 440Hz PCM16 blip next to the executable so the asset + audio stacks
/// get exercised end to end.
static void write_blip_wav(const std::filesystem::path& path)
{
    auto samples = std::vector<int16>();
    for (int i = 0; i < 4800; ++i)
    {
        const float t = static_cast<float>(i) / 48000.0f;
        const float envelope = 1.0f - static_cast<float>(i) / 4800.0f;
        samples.push_back(
            static_cast<int16>(std::sin(t * 440.0f * 6.28318f) * envelope * 20000.0f));
    }
    auto bytes = std::vector<std::byte>();
    auto push = [&bytes](const void* data, const size count)
    {
        const auto* raw = static_cast<const std::byte*>(data);
        bytes.insert(bytes.end(), raw, raw + count);
    };
    const uint32 data_size = static_cast<uint32>(samples.size() * 2);
    const uint32 riff = 36 + data_size;
    const uint16 format = 1, channels = 1, block = 2, bits = 16;
    const uint32 rate = 48000, byte_rate = rate * block;
    const uint32 fmt_size = 16;
    push("RIFF", 4);
    push(&riff, 4);
    push("WAVE", 4);
    push("fmt ", 4);
    push(&fmt_size, 4);
    push(&format, 2);
    push(&channels, 2);
    push(&rate, 4);
    push(&byte_rate, 4);
    push(&block, 2);
    push(&bits, 2);
    push("data", 4);
    push(&data_size, 4);
    push(samples.data(), data_size);
    if (const auto written = tbx::files::write_bytes(path, bytes); !written)
        tbx::log_warn("{}", written.error());
}

/// @brief
/// Purpose: Authors the world's kits (enemy prefab nested inside room kits) and opens the
/// layout: the hub is always loaded, the far room streams by distance.
static tbx::Result<void> open_world(
    tbx::Sandbox& sandbox,
    const tbx::AssetHandle<tbx::ScriptSource>& chase)
{
    using namespace tbx;
    auto author = Sandbox(get_jobs());

    Toy enemy =
        author.spawn("Enemy")
            .with(Transform {.position = Vec3(0.0f, 0.75f, 0.0f), .scale = Vec3(0.8f, 1.5f, 0.8f)})
            .with(MeshRenderer {.mesh = builtin::CUBE, .tint = colors::RED})
            .with(Collider {.half_extents = Vec3(0.4f, 0.75f, 0.4f)})
            .with(RigidBody {.is_kinematic = true})
            .with(Script {.source = chase})
            .sticker("enemy");
    const Json enemy_kit = save(author, std::array {enemy});
    author.despawn(enemy);

    auto make_room = [&author, &enemy_kit](const char* name, const bool with_enemy) -> Json
    {
        Toy floor = author.spawn(name)
                        .with(Transform {.scale = Vec3(16.0f, 1.0f, 16.0f)})
                        .with(MeshRenderer {.mesh = builtin::PLANE, .tint = colors::GRAY})
                        .with(Collider {.half_extents = Vec3(8.0f, 0.1f, 8.0f)});
        Toy pillar = author.spawn("Pillar")
                         .with(
                             Transform {
                                 .position = Vec3(3.0f, 1.0f, -3.0f),
                                 .scale = Vec3(1.0f, 2.0f, 1.0f)})
                         .with(MeshRenderer {.mesh = builtin::CUBE, .tint = colors::BLUE})
                         .with(Collider {.half_extents = Vec3(0.5f, 1.0f, 0.5f)});
        Json room = save(author, std::array {floor, pillar});
        if (with_enemy)
            room["kits"] =
                Json::array({Json {{"reference", "enemy"}, {"position", {0.0f, 0.0f, -6.0f}}}});
        author.despawn(floor);
        author.despawn(pillar);
        return room;
    };

    const Json hub = make_room("HubFloor", true);
    const Json far_room = make_room("FarFloor", true);
    const auto resolver =
        [kits =
             std::map<std::string, Json> {{"enemy", enemy_kit}, {"hub", hub}, {"far", far_room}}](
            const std::string& reference) -> Result<Json>
    {
        const auto it = kits.find(reference);
        if (it == kits.end())
            return fail("unknown kit '{}'", reference);
        return it->second;
    };

    auto layout = Json {
        {"kits",
         Json::array(
             {Json {{"reference", "hub"}, {"mode", "always"}},
              Json {
                  {"reference", "far"},
                  {"mode", "streamed"},
                  {"position", {0.0f, 0.0f, -60.0f}}}})}};
    return sandbox.open({.kits = layout, .resolver = resolver});
}

int main(int argc, char** argv)
{
    using namespace tbx;
    bool selftest = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--selftest") == 0)
            selftest = true;

    const auto asset_root = std::filesystem::path("doom_assets");
    write_blip_wav(asset_root / "blip.wav");

    auto app = App {.title = "Toybox Doom", .asset_root = asset_root};
    float yaw = 0.0f;
    float pitch = 0.0f;
    int kills = 0;
    uint64 hud = 0;
    auto blip = AssetHandle<AudioClip> {};
    Toy player = {};
    Toy shot_speaker = {};
    bool streamed_room_seen = false;
    size hub_toy_count = 0;

    while (run(app))
    {
        auto& sandbox = get_sandbox();

        if (app.frame == 1)
        {
            const auto chase = get_scripts().load_source("chase.luau", CHASE_SCRIPT);
            if (!chase)
            {
                log_error("chase script: {}", chase.error());
                return 1;
            }
            if (const auto world = open_world(sandbox, *chase); !world)
            {
                log_error("world setup failed: {}", world.error());
                return 1;
            }
            player = sandbox.spawn("Player")
                         .with(Transform {.position = Vec3(0.0f, 1.2f, 6.0f)})
                         .with(Camera {})
                         .with(AudioListener {});
            if (const auto document = ui::load_document(HUD_RML))
                hud = *document;
            get_jobs().start(
                []() -> Task<void>
                {
                    const auto loaded = co_await get_assets().load<AudioClip>("blip.wav");
                    if (!loaded)
                        log_warn("{}", loaded.error());
                }());
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

        // Selftest choreography: hold still, then fire at the hub enemy walking toward us,
        // then sprint toward the far room so streaming proves itself.
        bool fire = input::is_mouse_pressed(MouseButton::LEFT);
        if (selftest)
        {
            if (app.frame == 120)
                fire = true; // enemy has chased into the crosshair line by now
            if (app.frame > 130)
                transform.position += Vec3(0.0f, 0.0f, -0.5f); // warp toward the far room
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
                        hud,
                        "kills",
                        std::format(
                            "position: absolute; left: 16px; top: 16px; width: {}px; "
                            "height: 14px; background-color: #ff3333;",
                            kills * 40));
                }
                if (shot_speaker.is_alive())
                    sandbox.despawn(shot_speaker);
                shot_speaker = sandbox.spawn("Shot")
                                   .with(Transform {.position = hit->position})
                                   .with(AudioSource {.clip = blip, .is_playing = true});
            }
        }
        if (!blip.is_valid())
            blip = AssetHandle<AudioClip> {}; // resolved lazily once the async load lands

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
