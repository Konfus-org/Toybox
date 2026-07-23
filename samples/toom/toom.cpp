#include "tbx/app.h"
#include "tbx/utils/command_list.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/reflection/reflection.h"
#include "tbx/runtime.h"
#include "tbx/serialization/read_write.h"
#include "tbx/serialization/serializers.h"
#include <filesystem>

// Toom — the doom clone, fully data-driven AND fully scripted: the App declares the level and HUD,
// the level chain pulls in everything else (rooms, walls, materials, the sky and post chain,
// the player with scripts/player.luau, the enemy with scripts/enemy.luau). This file only
// renders and, in selftest, checks the outcome the scripts produced.

int main(int argc, char** argv)
{
    const auto commands = tbx::CommandList(argc, argv);
    const bool selftest = commands.has("selftest");

    // Everything about the app — window, entry sandbox, icon, subsystem settings — lives in
    // the .tapp; this file is only the loop and the selftest checks. The .tapp decodes
    // generically through the reflected App schema, so registration comes first.
    tbx::initialize_reflection();
    tbx::register_builtin_serializers();
    const auto tapp = std::filesystem::path(SAMPLE_ASSETS_PATH) / "Toom.tapp";
    auto loaded = tbx::deserialize<tbx::App>(tapp);
    if (!loaded)
    {
        TBX_ERROR("Toom.tapp: {}", loaded.error());
        return 1;
    }
    loaded->config.root_dir = tapp.parent_path(); // derived, never serialized
    loaded->commands = commands; // the runtime honors -w/-h and --screenshot

    bool scored = false;
    bool streamed_room_seen = false;

    auto runtime = tbx::Runtime(std::move(*loaded));
    while (tbx::run(runtime))
    {
        const uint64 frame = runtime.state->frame.index;
        auto& sandbox = runtime.state->sandbox;

        if (selftest)
        {
            // The "selftest" sticker tells player.luau to run the choreography: shoot the
            // hub enemy, then sprint north until the far room streams in.
            if (frame == 1)
            {
                auto player = sandbox.find("Player");
                if (!player)
                {
                    TBX_ERROR("levels/arena.box did not spawn a Player");
                    return 1;
                }
                // Fluent handle: mutators return the toy, so tagging and enabling chain.
                player->add("selftest").set_enabled(true);
            }
            if (auto player = sandbox.find("Player"); player && player->has("scored"))
                scored = true;
            if (sandbox.find("FarFloor"))
                streamed_room_seen = true;
            if (frame >= 300)
                tbx::quit(runtime);
        }
    }

    if (selftest)
    {
        const bool passed = scored && streamed_room_seen;
        TBX_INFO(
            "toom selftest: scored={} streamed_room={} -> {}",
            scored,
            streamed_room_seen,
            passed ? "PASSED" : "FAILED");
        return passed ? 0 : 1;
    }
    return 0;
}
