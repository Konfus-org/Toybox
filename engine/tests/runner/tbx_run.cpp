#include "tbx/debug/log.h"
#include "tbx/utils/typedefs.h"
#include "tbx/app.h"
#include "tbx/debug/debug_view.h"
#include "tbx/assets/builtin.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/gfx/gpu.h"
#include "tbx/ui/ui.h"
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


/// @brief
/// Purpose: A live-bound stat for the selftest's ui::bind smoke coverage.
struct SelftestStats
{
    int frames = 0;
};

static tbx::Quat look_toward(const tbx::Vec3& direction)
{
    const tbx::Vec3 up = std::abs(direction.y) > 0.99f ? tbx::Vec3(0.0f, 0.0f, -1.0f)
                                                       : tbx::Vec3(0.0f, 1.0f, 0.0f);
    return tbx::math::quat_look_at(tbx::math::normalize(direction), up);
}

// Renders a plane + floating cube + angled sun and verifies by pixel readback that the cube is
// lit red and that its cast shadow darkens the plane — the M6 "lit/shadowed scene" proof.
static int run_scene_selftest()
{
    // The engine resources folder doubles as the asset root: the red cube's material is an
    // engine-shipped asset (color comes from materials now, not renderer tints).
    auto app = tbx::App {.title = "Toybox 2 scene", .asset_root = TBX_RESOURCES_PATH};
    float shadowed_brightness = -1.0f;
    float unshadowed_brightness = -1.0f;
    bool cube_is_red = false;
    bool ui_panel_visible = false;
    bool reflection_works = false;
    tbx::Toy camera = {};
    auto stats = SelftestStats {};
    tbx::ui::bind(stats.frames, "scene_frames");

    while (tbx::run(app))
    {
        stats.frames = static_cast<int>(app.frame);
        auto& sandbox = tbx::get_sandbox();
        if (app.frame == 1)
        {
            sandbox.spawn("Ground")
                .with(tbx::Transform {.scale = tbx::Vec3(60.0f, 1.0f, 60.0f)})
                .with(tbx::Renderer {.model = tbx::builtin::PLANE});
            sandbox.spawn("Cube")
                .with(tbx::Transform {.position = tbx::Vec3(0.0f, 2.0f, 0.0f)})
                .with(tbx::Renderer {
                    .material = tbx::AssetHandle<tbx::Material>("Materials/Tbx/red.mat"),
                    .model = tbx::builtin::CUBE});
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

        if (app.frame == 1)
            tbx::debug::set_open(true); // exercised alongside the scene: text + overlay path
        static const auto ui_panel = tbx::UiDocument {.text = R"(<rml>
<head><style>
body { width: 100%; height: 100%; }
#panel { position: absolute; left: 0px; top: 0px; width: 220px; height: 220px;
         background-color: #00ff00; }
#frames { position: absolute; left: 400px; top: 8px; font-family: Montserrat;
          font-size: 14px; color: #ffffff; }
</style></head>
<body><div id="panel"/><div id="frames" data-text="scene_frames"/></body>
</rml>)"};
        tbx::gpu::render(sandbox); // owns begin_frame; the ui pass renders Ui blocks
        tbx::ui::draw(ui_panel); // immediate: renders right now, on top of the frame

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
        if (app.frame == 6)
        {
            // The UI panel owns the top-left corner (GL readback is y-up).
            const tbx::Color corner = tbx::gpu::read_pixel(60, window.get_height() - 60);
            ui_panel_visible = corner.g > 0.8f && corner.r < 0.2f;
        }

        if (app.frame >= 6)
            tbx::quit();
    }

    const bool shadow_darkens = unshadowed_brightness > shadowed_brightness + 0.5f;
    const bool passed = cube_is_red && shadow_darkens && reflection_works && ui_panel_visible;
    TBX_INFO(
        "scene selftest: cube_red={} shadowed={:.2f} lit={:.2f} reflection={} ui={} -> {}",
        cube_is_red,
        shadowed_brightness,
        unshadowed_brightness,
        reflection_works,
        ui_panel_visible,
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
    std::unique_ptr<tbx::gpu::Pipeline> pipeline = {};
    std::unique_ptr<tbx::gpu::Mesh> mesh = {};

    while (tbx::run(app))
    {
        if (!shader)
        {
            auto compiled =
                tbx::gpu::compile_shader(TRIANGLE_VERTEX_SHADER, TRIANGLE_FRAGMENT_SHADER);
            if (!compiled)
            {
                TBX_ERROR("{}", compiled.error());
                return 1;
            }
            shader = std::move(*compiled);
            pipeline = tbx::gpu::make_pipeline({.shader = *shader});
            mesh = tbx::gpu::upload_mesh(TRIANGLE_VERTICES, std::array {3, 4});
        }

        tbx::gpu::begin_frame();
        tbx::gpu::set_pipeline(*pipeline);
        tbx::gpu::draw(*mesh);

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
    pipeline.reset();
    shader.reset();
    mesh.reset();

    if (selftest)
    {
        TBX_INFO("selftest {}", selftest_passed ? "PASSED" : "FAILED");
        return selftest_passed ? 0 : 1;
    }
    return 0;
}
