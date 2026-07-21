#include "tbx/app.h"
#include "tbx/debug/log.h"
#include "tbx/gfx/gpu.h"
#include <cstring>

// Toom — the doom clone, fully data-driven AND fully scripted: the App declares the level and HUD,
// the level chain pulls in everything else (rooms, walls, materials, the sky and post chain,
// the player with scripts/player.luau, the enemy with scripts/enemy.luau). This file only
// renders and, in selftest, checks the outcome the scripts produced.

int main(int argc, char** argv)
{
    using namespace tbx;
    bool selftest = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--selftest") == 0)
            selftest = true;

    auto app = App {
        .title = "Toom",
        .asset_root = SAMPLE_ASSETS_PATH,
        .sandbox = AssetHandle<Json>("levels/arena.box"),
        .icon = AssetHandle<Texture>("textures/ToomLogo.jpg")};
    bool scored = false;
    bool streamed_room_seen = false;

    while (run(app))
    {
        auto& sandbox = get_sandbox();

        if (selftest)
        {
            // The "selftest" sticker tells player.luau to run the choreography: shoot the
            // hub enemy, then sprint north until the far room streams in.
            if (app.frame == 1)
            {
                auto player = sandbox.find("Player");
                if (!player)
                {
                    log_error("levels/arena.box did not spawn a Player");
                    return 1;
                }
                player->sticker("selftest");
            }
            if (auto player = sandbox.find("Player"); player && player->has_sticker("scored"))
                scored = true;
            if (sandbox.find("FarFloor"))
                streamed_room_seen = true;
            if (app.frame >= 300)
                quit();
        }

        tbx::gpu::render(sandbox);
    }

    if (selftest)
    {
        const bool passed = scored && streamed_room_seen;
        log_info(
            "toom selftest: scored={} streamed_room={} -> {}",
            scored,
            streamed_room_seen,
            passed ? "PASSED" : "FAILED");
        return passed ? 0 : 1;
    }
    return 0;
}
