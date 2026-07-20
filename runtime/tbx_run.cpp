#include "tbx/core/log.h"
#include "tbx/core/typedefs.h"
#include "tbx/app.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/render_blocks.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
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


static tbx::Quat look_toward(const tbx::Vec3& direction)
{
    const tbx::Vec3 up = std::abs(direction.y) > 0.99f ? tbx::Vec3(0.0f, 0.0f, -1.0f)
                                                       : tbx::Vec3(0.0f, 1.0f, 0.0f);
    return glm::quatLookAt(glm::normalize(direction), up);
}

// Renders a plane + floating cube + angled sun and verifies by pixel readback that the cube is
// lit red and that its cast shadow darkens the plane — the M6 "lit/shadowed scene" proof.
static int run_scene_selftest()
{
    auto app = tbx::App {.title = "Toybox 2 scene"};
    float shadowed_brightness = -1.0f;
    float unshadowed_brightness = -1.0f;
    bool cube_is_red = false;
    bool reflection_works = false;
    tbx::Toy camera = {};

    while (tbx::run(app))
    {
        auto& sandbox = tbx::get_sandbox();
        if (app.frame == 1)
        {
            sandbox.spawn("Ground")
                .with(tbx::Transform {.scale = tbx::Vec3(60.0f, 1.0f, 60.0f)})
                .with(tbx::MeshRenderer {.mesh = "plane", .tint = tbx::Color {}});
            sandbox.spawn("Cube")
                .with(tbx::Transform {.position = tbx::Vec3(0.0f, 2.0f, 0.0f)})
                .with(tbx::MeshRenderer {
                    .mesh = "cube",
                    .tint = tbx::Color {.r = 1.0f, .g = 0.1f, .b = 0.1f}});
            sandbox.spawn("Sun")
                .with(tbx::Transform {
                    .rotation = look_toward(tbx::Vec3(1.0f, -1.0f, 0.0f))})
                .with(tbx::DirectionalLight {.intensity = 1.0f});
            camera = sandbox.spawn("Camera")
                         .with(tbx::Transform {.position = tbx::Vec3(0.0f, 2.0f, 8.0f)})
                         .with(tbx::Camera {});
        }

        // Probe positions: cube face, the shadow spot (+2,0,0), a matching lit spot (-2,0,0).
        if (app.frame == 3)
            camera.get_block<tbx::Transform>() = tbx::Transform {
                .position = tbx::Vec3(2.0f, 10.0f, 0.0f),
                .rotation = look_toward(tbx::Vec3(0.0f, -1.0f, 0.0f))};
        if (app.frame == 5)
            camera.get_block<tbx::Transform>() = tbx::Transform {
                .position = tbx::Vec3(-2.0f, 10.0f, 0.0f),
                .rotation = look_toward(tbx::Vec3(0.0f, -1.0f, 0.0f))};

        tbx::gpu::begin_frame();
        tbx::gpu::render(sandbox);

        const auto& window = tbx::get_window();
        const tbx::Color center =
            tbx::gpu::read_pixel(window.get_width() / 2, window.get_height() / 2);
        if (app.frame == 2)
        {
            cube_is_red = center.r > 0.25f && center.r > center.g * 2.0f;
    // Shader reflection: an arbitrary shader's uniform schema is discoverable and drivable.
        {
            auto probe = tbx::gpu::compile_shader(
                R"(#version 460 core
uniform mat4 u_model;
uniform vec4 u_tint;
uniform float u_shine;
void main() { gl_Position = u_model * vec4(u_shine, u_tint.x, 0.0, 1.0); })",
                R"(#version 460 core
out vec4 c; void main() { c = vec4(1.0); })");
            if (probe)
            {
                const auto info = tbx::gpu::reflect(**probe);
                auto found = 0;
                for (const auto& uniform : info.uniforms)
                {
                    if (uniform.name == "u_model" && uniform.kind == tbx::gpu::UniformKind::MAT4)
                        ++found;
                    if (uniform.name == "u_tint" && uniform.kind == tbx::gpu::UniformKind::VEC4)
                        ++found;
                    if (uniform.name == "u_shine" && uniform.kind == tbx::gpu::UniformKind::FLOAT)
                        ++found;
                }
                tbx::gpu::apply_uniforms(
                    **probe,
                    tbx::Json {{"u_tint", {1.0, 0.0, 0.0, 1.0}}, {"u_shine", 0.5}});
                reflection_works = found == 3;
            }
        }
        }
        if (app.frame == 4)
            shadowed_brightness = center.r + center.g + center.b;
        if (app.frame == 6)
            unshadowed_brightness = center.r + center.g + center.b;

        if (app.frame >= 6)
            tbx::quit();
    }

    const bool shadow_darkens = unshadowed_brightness > shadowed_brightness + 0.5f;
    const bool passed = cube_is_red && shadow_darkens && reflection_works;
    tbx::log_info(
        "scene selftest: cube_red={} shadowed={:.2f} lit={:.2f} reflection={} -> {}",
        cube_is_red,
        shadowed_brightness,
        unshadowed_brightness,
        reflection_works,
        passed ? "PASSED" : "FAILED");
    return passed ? 0 : 1;
}

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
        else if (std::strcmp(argv[i], "--scene-selftest") == 0)
            return run_scene_selftest();
    }

    auto app = tbx::App {.title = "Toybox 2"};
    bool selftest_passed = false;
    std::unique_ptr<tbx::gpu::Shader> shader = {};
    std::unique_ptr<tbx::gpu::Mesh> mesh = {};

    while (tbx::run(app))
    {
        if (!shader)
        {
            auto compiled =
                tbx::gpu::compile_shader(TRIANGLE_VERTEX_SHADER, TRIANGLE_FRAGMENT_SHADER);
            if (!compiled)
            {
                tbx::log_error("{}", compiled.error());
                return 1;
            }
            shader = std::move(*compiled);
            mesh = tbx::gpu::upload_mesh(TRIANGLE_VERTICES, std::array {3, 4});
        }

        tbx::gpu::begin_frame();
        tbx::gpu::draw(*shader, *mesh);

        if (selftest)
        {
            // The triangle covers the framebuffer center; the clear color does not.
            const auto& window = tbx::get_window();
            const tbx::Color center =
                tbx::gpu::read_pixel(window.get_width() / 2, window.get_height() / 2);
            const tbx::Color corner = tbx::gpu::read_pixel(2, 2);
            const bool center_is_triangle = center.r + center.g + center.b > 0.5f;
            const bool corner_is_clear = std::abs(corner.r - 0.08f) < 0.02f;
            selftest_passed = center_is_triangle && corner_is_clear;
        }

        if (frame_limit >= 0 && app.frame >= static_cast<uint64>(frame_limit))
            tbx::quit();
    }

    // GPU resources must die before run() tears the context down... they already did not:
    // release them explicitly before exit since the loop ended with the context gone.
    shader.reset();
    mesh.reset();

    if (selftest)
    {
        tbx::log_info("selftest {}", selftest_passed ? "PASSED" : "FAILED");
        return selftest_passed ? 0 : 1;
    }
    return 0;
}
