#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/runtime.h"
#include "tbx/scene.h"
#include "tbx/utils/command_list.h"
#include <filesystem>

// Toom — the doom clone, fully data-driven AND fully scripted: the App declares the level and HUD,
// the level chain pulls in everything else (rooms, walls, materials, the sky and post chain,
// the player with scripts/player.luau, the enemy with scripts/enemy.luau). This file only
// renders and, in selftest, checks the outcome the scripts produced.

int main(int argc, char** argv)
{
    const auto commands = tbx::CommandList(argc, argv);
    const bool selftest = commands.has("selftest");
    if (selftest)
        TBX_INFO("Running selftest!");

    // Everything about the app — window, entry sandbox, icon, subsystem settings — lives in the
    // .tapp. run() loads and applies it (standing reflection/assets up itself), so main just hands
    // the runtime a handle to the .tapp (its folder is the asset root) plus the parsed command line.
    const auto tapp = std::filesystem::path(SAMPLE_ASSETS_PATH) / "Toom.tapp";
    auto runtime = tbx::Runtime(tbx::AssetHandle<tbx::App>(tapp.string()), commands);

    bool scored = false;
    bool streamed_room_seen = false;
    // The game reaches the world through the free-function API (tbx::find), never the runtime
    // state.
    uint64 frame = 0;
    while (tbx::run(runtime))
    {
        ++frame;
        if (selftest)
        {
            // The "selftest" sticker tells player.luau to run the choreography: shoot the
            // hub enemy, then sprint north until the far room streams in.
            if (frame == 1)
            {
                auto player = tbx::find("Player");
                if (!player)
                {
                    TBX_ERROR("levels/arena.box did not spawn a Player");
                    return 1;
                }
                // Fluent handle: mutators return the toy, so tagging and enabling chain.
                player->add("selftest").set_enabled(true);
            }
            if (auto player = tbx::find("Player"); player && player->has("scored"))
                scored = true;
            if (tbx::find("FarFloor"))
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
