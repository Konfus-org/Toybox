#include "tbx/engine.h"
#include "tbx/core/log.h"
#include "tbx/gfx/gpu.h"

namespace tbx
{
    //// ENGINE ////

    Engine::Engine(const EngineConfig& config)
        : window(
              WindowDesc {
                  .title = config.title,
                  .width = config.width,
                  .height = config.height,
                  .headless = config.headless})
    {
        if (!window.headless())
            gpu::init(window.gl_proc_loader());
        log_info(
            "Toybox engine up ({}x{}{})",
            window.width(),
            window.height(),
            window.headless() ? ", headless" : "");
    }

    Engine::~Engine() = default;

    void Engine::begin_frame()
    {
        if (window.headless())
            return;
        gpu::set_viewport(window.width(), window.height());
        gpu::clear({.r = 0.08f, .g = 0.08f, .b = 0.10f, .a = 1.0f});
    }

    bool Engine::pump()
    {
        input.new_frame();
        const bool alive = window.pump(input, events);
        jobs.drain_main();
        events.drain();
        return alive;
    }

    void Engine::render()
    {
        if (window.headless())
            return;
        window.swap();
    }

    void Engine::update(float dt)
    {
        _fixed_accumulator += dt;
        while (_fixed_accumulator >= FIXED_STEP)
        {
            _fixed_accumulator -= FIXED_STEP;
            // Fixed-cadence work lands here with later milestones (physics, scripts).
        }
    }
}
