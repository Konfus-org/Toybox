#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include <cstring>
#include <filesystem>

// Toom — the doom clone, fully data-driven AND fully scripted: the App declares the level and HUD,
// the level chain pulls in everything else (rooms, walls, materials, the sky and post chain,
// the player with scripts/player.luau, the enemy with scripts/enemy.luau). This file only
// renders and, in selftest, checks the outcome the scripts produced.

int main(int argc, char** argv)
{
    bool selftest = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--selftest") == 0)
            selftest = true;

    // Everything about the app — window, entry sandbox, icon, subsystem settings — lives in
    // the .tapp; this file is only the loop and the selftest checks.
    auto loaded = tbx::load_app(std::filesystem::path(SAMPLE_ASSETS_PATH) / "Toom.tapp");
    if (!loaded)
    {
        TBX_ERROR("Toom.tapp: {}", loaded.error());
        return 1;
    }
    tbx::App app = std::move(*loaded);

    bool scored = false;
    bool streamed_room_seen = false;

    while (tbx::run(app))
    {
        auto& sandbox = tbx::get_sandbox();

        if (selftest)
        {
            // The "selftest" sticker tells player.luau to run the choreography: shoot the
            // hub enemy, then sprint north until the far room streams in.
            if (app.state.frame == 1)
            {
                auto player = sandbox.find("Player");
                if (!player)
                {
                    TBX_ERROR("levels/arena.box did not spawn a Player");
                    return 1;
                }
                player->sticker("selftest");
            }
            if (auto player = sandbox.find("Player"); player && player->has_sticker("scored"))
                scored = true;
            if (sandbox.find("FarFloor"))
                streamed_room_seen = true;
            if (app.state.frame >= 300)
                tbx::quit();
        }

        tbx::gpu::render(sandbox);
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
