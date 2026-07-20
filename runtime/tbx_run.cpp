#include "tbx/core/log.h"
#include "tbx/engine.h"
#include "tbx/gfx/gpu.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>

static constexpr const char* TRIANGLE_VERTEX_SHADER = R"(#version 460 core
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;
out vec4 v_color;
void main()
{
    v_color = in_color;
    gl_Position = vec4(in_position, 1.0);
})";

static constexpr const char* TRIANGLE_FRAGMENT_SHADER = R"(#version 460 core
in vec4 v_color;
out vec4 out_color;
void main()
{
    out_color = v_color;
})";

// position (vec3) + color (vec4), one big centered triangle
static constexpr std::array<float, 21> TRIANGLE_VERTICES = {
    // x      y      z     r     g     b     a
    -0.6f, -0.5f, 0.0f, 1.0f, 0.2f, 0.2f, 1.0f, //
    0.6f,  -0.5f, 0.0f, 0.2f, 1.0f, 0.2f, 1.0f, //
    0.0f,  0.7f,  0.0f, 0.2f, 0.2f, 1.0f, 1.0f, //
};

int main(int argc, char** argv)
{
    int frame_limit = -1; // run until the window closes
    bool selftest = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            frame_limit = std::stoi(argv[++i]);
        else if (std::strcmp(argv[i], "--selftest") == 0)
        {
            selftest = true;
            if (frame_limit < 0)
                frame_limit = 10;
        }
    }

    auto engine = tbx::Engine(tbx::EngineConfig {.title = "Toybox 2"});

    auto shader = tbx::gpu::create_shader(TRIANGLE_VERTEX_SHADER, TRIANGLE_FRAGMENT_SHADER);
    if (!shader)
    {
        tbx::log_error("{}", shader.error());
        return 1;
    }
    const auto mesh = tbx::gpu::create_mesh(TRIANGLE_VERTICES, std::array {3, 4});

    bool selftest_passed = false;
    auto previous = std::chrono::steady_clock::now();
    for (int frame = 0; frame_limit < 0 || frame < frame_limit; ++frame)
    {
        if (!engine.pump())
            break;

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - previous).count();
        previous = now;
        engine.update(dt);

        engine.begin_frame();
        tbx::gpu::draw(*shader, mesh);

        if (selftest)
        {
            // The triangle covers the framebuffer center; the clear color does not.
            const tbx::Color center =
                tbx::gpu::read_pixel(engine.window.width() / 2, engine.window.height() / 2);
            const tbx::Color corner = tbx::gpu::read_pixel(2, 2);
            const bool center_is_triangle = center.r + center.g + center.b > 0.5f;
            const bool corner_is_clear = std::abs(corner.r - 0.08f) < 0.02f;
            selftest_passed = center_is_triangle && corner_is_clear;
        }

        engine.render();
    }

    tbx::gpu::destroy_mesh(mesh);
    tbx::gpu::destroy_shader(*shader);

    if (selftest)
    {
        tbx::log_info("selftest {}", selftest_passed ? "PASSED" : "FAILED");
        return selftest_passed ? 0 : 1;
    }
    return 0;
}
