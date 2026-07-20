#include "tbx/core/log.h"
#include "tbx/ecs/block.h"
#include "tbx/gfx/gpu.h"
#include "tbx/gfx/render_blocks.h"

namespace tbx::gpu
{
    // Shader source is GLSL for now — when a second gfx backend lands, sources move behind the
    // backend seam alongside gpu.h's implementations.
    static constexpr const char* DEPTH_VERTEX_SHADER = R"(#version 460 core
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
uniform mat4 u_model;
uniform mat4 u_light_view_projection;
void main()
{
    gl_Position = u_light_view_projection * u_model * vec4(in_position, 1.0);
})";

    static constexpr const char* DEPTH_FRAGMENT_SHADER = R"(#version 460 core
void main() {}
)";

    static constexpr const char* LIT_VERTEX_SHADER = R"(#version 460 core
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform mat4 u_light_view_projection;
out vec3 v_world_normal;
out vec4 v_shadow_coords;
void main()
{
    vec4 world = u_model * vec4(in_position, 1.0);
    v_world_normal = mat3(u_model) * in_normal;
    v_shadow_coords = u_light_view_projection * world;
    gl_Position = u_view_projection * world;
})";

    static constexpr const char* LIT_FRAGMENT_SHADER = R"(#version 460 core
in vec3 v_world_normal;
in vec4 v_shadow_coords;
uniform vec4 u_tint;
uniform vec4 u_light_color;
uniform float u_light_intensity;
uniform vec3 u_light_direction;
uniform sampler2D u_shadow_map;
out vec4 out_color;
void main()
{
    vec3 normal = normalize(v_world_normal);
    float lambert = max(dot(normal, -u_light_direction), 0.0);

    vec3 shadow = v_shadow_coords.xyz / v_shadow_coords.w * 0.5 + 0.5;
    float shadowing = 1.0;
    if (shadow.z <= 1.0)
    {
        float closest = texture(u_shadow_map, shadow.xy).r;
        if (shadow.z - 0.002 > closest)
            shadowing = 0.0;
    }

    float ambient = 0.25;
    float light = ambient + lambert * shadowing * u_light_intensity;
    out_color = vec4(u_tint.rgb * u_light_color.rgb * light, u_tint.a);
})";

    // Interleaved position + normal, 36 vertices.
    static constexpr float CUBE_VERTICES[] = {
        // +Z
        -0.5f, -0.5f, 0.5f, 0, 0, 1, 0.5f, -0.5f, 0.5f, 0, 0, 1, 0.5f, 0.5f, 0.5f, 0, 0, 1,
        -0.5f, -0.5f, 0.5f, 0, 0, 1, 0.5f, 0.5f, 0.5f, 0, 0, 1, -0.5f, 0.5f, 0.5f, 0, 0, 1,
        // -Z
        0.5f, -0.5f, -0.5f, 0, 0, -1, -0.5f, -0.5f, -0.5f, 0, 0, -1, -0.5f, 0.5f, -0.5f, 0, 0, -1,
        0.5f, -0.5f, -0.5f, 0, 0, -1, -0.5f, 0.5f, -0.5f, 0, 0, -1, 0.5f, 0.5f, -0.5f, 0, 0, -1,
        // +X
        0.5f, -0.5f, 0.5f, 1, 0, 0, 0.5f, -0.5f, -0.5f, 1, 0, 0, 0.5f, 0.5f, -0.5f, 1, 0, 0,
        0.5f, -0.5f, 0.5f, 1, 0, 0, 0.5f, 0.5f, -0.5f, 1, 0, 0, 0.5f, 0.5f, 0.5f, 1, 0, 0,
        // -X
        -0.5f, -0.5f, -0.5f, -1, 0, 0, -0.5f, -0.5f, 0.5f, -1, 0, 0, -0.5f, 0.5f, 0.5f, -1, 0, 0,
        -0.5f, -0.5f, -0.5f, -1, 0, 0, -0.5f, 0.5f, 0.5f, -1, 0, 0, -0.5f, 0.5f, -0.5f, -1, 0, 0,
        // +Y
        -0.5f, 0.5f, 0.5f, 0, 1, 0, 0.5f, 0.5f, 0.5f, 0, 1, 0, 0.5f, 0.5f, -0.5f, 0, 1, 0,
        -0.5f, 0.5f, 0.5f, 0, 1, 0, 0.5f, 0.5f, -0.5f, 0, 1, 0, -0.5f, 0.5f, -0.5f, 0, 1, 0,
        // -Y
        -0.5f, -0.5f, -0.5f, 0, -1, 0, 0.5f, -0.5f, -0.5f, 0, -1, 0, 0.5f, -0.5f, 0.5f, 0, -1, 0,
        -0.5f, -0.5f, -0.5f, 0, -1, 0, 0.5f, -0.5f, 0.5f, 0, -1, 0, -0.5f, -0.5f, 0.5f, 0, -1, 0};

    // A unit plane facing +Y (two triangles, wound for +Y visibility).
    static constexpr float PLANE_VERTICES[] = {
        -0.5f, 0.0f, 0.5f, 0, 1, 0, 0.5f, 0.0f, 0.5f, 0, 1, 0, 0.5f, 0.0f, -0.5f, 0, 1, 0,
        -0.5f, 0.0f, 0.5f, 0, 1, 0, 0.5f, 0.0f, -0.5f, 0, 1, 0, -0.5f, 0.0f, -0.5f, 0, 1, 0};

    /// @brief
    /// Purpose: Lazily-built renderer resources (RAII; released at process exit).
    struct RendererState
    {
        std::unique_ptr<Shader> depth_shader;
        std::unique_ptr<Shader> lit_shader;
        std::unique_ptr<Mesh> cube;
        std::unique_ptr<Mesh> plane;
        std::unique_ptr<DepthTarget> shadow_target;
    };

    static RendererState g_renderer = {};

    //// SETUP ////

    void register_render_blocks()
    {
        static bool g_registered = false;
        if (g_registered)
            return;
        g_registered = true;
        register_block<Camera>("Camera")
            .field("fov_degrees", &Camera::fov_degrees)
            .field("near_plane", &Camera::near_plane)
            .field("far_plane", &Camera::far_plane)
            .field("is_active", &Camera::is_active);
        register_block<MeshRenderer>("MeshRenderer")
            .field("mesh", &MeshRenderer::mesh)
            .field("tint", &MeshRenderer::tint);
        register_block<DirectionalLight>("DirectionalLight")
            .field("color", &DirectionalLight::color)
            .field("intensity", &DirectionalLight::intensity);
    }

    static bool ensure_renderer_ready()
    {
        if (g_renderer.lit_shader)
            return true;
        register_render_blocks();
        auto depth = compile_shader(DEPTH_VERTEX_SHADER, DEPTH_FRAGMENT_SHADER);
        auto lit = compile_shader(LIT_VERTEX_SHADER, LIT_FRAGMENT_SHADER);
        if (!depth || !lit)
        {
            log_error("renderer shaders failed: {}", depth ? lit.error() : depth.error());
            return false;
        }
        g_renderer.depth_shader = std::move(*depth);
        g_renderer.lit_shader = std::move(*lit);
        g_renderer.cube = upload_mesh(CUBE_VERTICES, std::array {3, 3});
        g_renderer.plane = upload_mesh(PLANE_VERTICES, std::array {3, 3});
        g_renderer.shadow_target = make_depth_target(2048);
        return true;
    }

    static const Mesh& mesh_by_name(const std::string& name)
    {
        if (name == "plane")
            return *g_renderer.plane;
        return *g_renderer.cube;
    }

    //// RENDER ////

    void render(Sandbox& sandbox)
    {
        if (!ensure_renderer_ready())
            return;
        auto& registry = sandbox.get_registry();

        // The first active camera wins; no camera, no picture.
        auto view_projection = Mat4(1.0f);
        bool has_camera = false;
        for (const auto [entity, camera] : registry.view<Camera>().each())
        {
            if (!camera.is_active)
                continue;
            const Mat4 world = sandbox.get_world_matrix(Toy(sandbox, entity));
            const float aspect = get_viewport_height() > 0
                ? static_cast<float>(get_viewport_width()) / get_viewport_height()
                : 1.0f;
            const Mat4 projection = glm::perspective(
                glm::radians(camera.fov_degrees), aspect, camera.near_plane, camera.far_plane);
            view_projection = projection * glm::inverse(world);
            has_camera = true;
            break;
        }
        if (!has_camera)
            return;

        // The first directional light is the sun; light looks along its -Z.
        auto light_direction = glm::normalize(Vec3(-0.4f, -1.0f, -0.3f));
        auto light_color = Color {};
        float light_intensity = 1.0f;
        for (const auto [entity, light] : registry.view<DirectionalLight>().each())
        {
            const Mat4 world = sandbox.get_world_matrix(Toy(sandbox, entity));
            light_direction = glm::normalize(Vec3(world * Vec4(0.0f, 0.0f, -1.0f, 0.0f)));
            light_color = light.color;
            light_intensity = light.intensity;
            break;
        }
        const Mat4 light_view = glm::lookAt(
            -light_direction * 30.0f,
            Vec3(0.0f, 0.0f, 0.0f),
            Vec3(0.0f, 1.0f, 0.0f));
        const Mat4 light_view_projection =
            glm::ortho(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 100.0f) * light_view;

        // Pass 1: depth from the light.
        begin_depth_pass(*g_renderer.shadow_target);
        set_uniform(*g_renderer.depth_shader, "u_light_view_projection", light_view_projection);
        for (const auto [entity, renderer] : registry.view<MeshRenderer>().each())
        {
            set_uniform(
                *g_renderer.depth_shader,
                "u_model",
                sandbox.get_world_matrix(Toy(sandbox, entity)));
            draw(*g_renderer.depth_shader, mesh_by_name(renderer.mesh));
        }
        end_depth_pass();

        // Pass 2: lit + shadowed.
        const Shader& lit = *g_renderer.lit_shader;
        set_uniform(lit, "u_view_projection", view_projection);
        set_uniform(lit, "u_light_view_projection", light_view_projection);
        set_uniform(lit, "u_light_direction", light_direction);
        set_uniform(lit, "u_light_color", light_color);
        set_uniform(lit, "u_light_intensity", light_intensity);
        set_uniform(lit, "u_shadow_map", 0);
        bind_depth_texture(*g_renderer.shadow_target, 0);
        for (const auto [entity, renderer] : registry.view<MeshRenderer>().each())
        {
            set_uniform(lit, "u_model", sandbox.get_world_matrix(Toy(sandbox, entity)));
            set_uniform(lit, "u_tint", renderer.tint);
            draw(lit, mesh_by_name(renderer.mesh));
        }
    }
}
