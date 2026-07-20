#include "tbx/engine.h"
#include "tbx/core/log.h"
#include "tbx/gfx/gpu.h"
#include "tbx/platform/input.h"

namespace tbx
{
    //// ENGINE ////

    Engine::Engine(const EngineConfig& config)
        : window(
              WindowDescription {
                  .title = config.title,
                  .width = config.width,
                  .height = config.height,
                  .is_headless = config.is_headless})
        , sandbox(jobs)
        , scripts(sandbox, events)
    {
        if (!window.is_headless())
            gpu::initialize();
        log_info(
            "Toybox engine up ({}x{}{})",
            window.get_width(),
            window.get_height(),
            window.is_headless() ? ", headless" : "");
    }

    Engine::~Engine() = default;

    void Engine::begin_frame()
    {
        if (window.is_headless())
            return;
        gpu::set_viewport(window.get_width(), window.get_height());
        gpu::clear({.r = 0.08f, .g = 0.08f, .b = 0.10f, .a = 1.0f});
    }

    bool Engine::pump()
    {
        input::pump();
        const bool alive = window.pump(events);
        jobs.drain_main();
        events.drain();
        return alive;
    }

    void Engine::render()
    {
        if (window.is_headless())
            return;
        window.swap();
    }

    void Engine::update(float delta_time)
    {
        sandbox.process_streaming();
        scripts.update(delta_time);
        _fixed_accumulator += delta_time;
        while (_fixed_accumulator >= FIXED_STEP)
        {
            _fixed_accumulator -= FIXED_STEP;
            // Fixed-cadence work lands here with later milestones (physics, scripts).
        }
    }
}
