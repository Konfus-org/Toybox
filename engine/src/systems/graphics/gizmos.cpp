#include "tbx/systems/graphics/gizmos.h"
#include "tbx/systems/debugging/macros.h"
#include "tbx/types/assets/material.h"
#include "tbx/types/assets/shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>

namespace tbx
{
    // Full turn and half turn in radians, used to parameterize the generated ring/torus/sphere arcs.
    constexpr float TWO_PI = 6.2831853F;
    constexpr float PI = 3.14159265F;

    // Two unit vectors spanning the plane perpendicular to `axis` (assumed roughly unit length).
    static void perpendicular_basis(const glm::vec3& axis, glm::vec3& out_u, glm::vec3& out_v)
    {
        const auto reference =
            std::abs(axis.y) < 0.99F ? glm::vec3(0.0F, 1.0F, 0.0F) : glm::vec3(1.0F, 0.0F, 0.0F);
        out_u = glm::normalize(glm::cross(axis, reference));
        out_v = glm::cross(axis, out_u);
    }

    Gizmos::Gizmos(std::weak_ptr<IGraphicsBackend> backend, std::weak_ptr<AssetManager> assets)
        : _backend(std::move(backend))
        , _assets(std::move(assets))
    {
    }

    Gizmos::~Gizmos() noexcept = default;

    void Gizmos::clear()
    {
        auto lock = std::lock_guard(_mutex);
        _lines.clear();
        _triangles.clear();
    }

    void Gizmos::set_color(const Color& color)
    {
        _color = color;
    }

    void Gizmos::set_matrix(const Mat4& matrix)
    {
        _matrix = matrix;
        _has_matrix = true;
    }

    void Gizmos::reset_matrix()
    {
        _matrix = Mat4(1.0F);
        _has_matrix = false;
    }

    Vec3 Gizmos::transform_point(const Vec3& point) const
    {
        return _has_matrix ? Vec3(_matrix * Vec4(point, 1.0F)) : point;
    }

    void Gizmos::push_line(const Vec3& a, const Vec3& b)
    {
        push_line(a, b, _color);
    }

    void Gizmos::push_line(const Vec3& a, const Vec3& b, const Color& color)
    {
        const auto rgba = Vec4(color.r, color.g, color.b, color.a);
        const auto pa = transform_point(a);
        const auto pb = transform_point(b);
        auto lock = std::lock_guard(_mutex);
        _lines.push_back(GizmoVertex {.position = Vec4(pa, 1.0F), .color = rgba});
        _lines.push_back(GizmoVertex {.position = Vec4(pb, 1.0F), .color = rgba});
    }

    void Gizmos::push_triangle(const Vec3& a, const Vec3& b, const Vec3& c)
    {
        push_triangle(a, b, c, _color);
    }

    void Gizmos::push_triangle(const Vec3& a, const Vec3& b, const Vec3& c, const Color& color)
    {
        const auto rgba = Vec4(color.r, color.g, color.b, color.a);
        const auto pa = transform_point(a);
        const auto pb = transform_point(b);
        const auto pc = transform_point(c);
        auto lock = std::lock_guard(_mutex);
        _triangles.push_back(GizmoVertex {.position = Vec4(pa, 1.0F), .color = rgba});
        _triangles.push_back(GizmoVertex {.position = Vec4(pb, 1.0F), .color = rgba});
        _triangles.push_back(GizmoVertex {.position = Vec4(pc, 1.0F), .color = rgba});
    }

    void Gizmos::line(const Vec3& start, const Vec3& end)
    {
        push_line(start, end);
    }

    void Gizmos::ray(const Vec3& origin, const Vec3& direction, float length)
    {
        push_line(origin, origin + (glm::normalize(direction) * length));
    }

    void Gizmos::wire_box(const Vec3& center, const Vec3& size, const Quat& rotation)
    {
        const auto half = size * 0.5F;
        glm::vec3 corner[8];
        for (auto i = 0; i < 8; ++i)
        {
            const auto local = glm::vec3(
                (i & 1) ? half.x : -half.x, (i & 2) ? half.y : -half.y, (i & 4) ? half.z : -half.z);
            corner[i] = center + (rotation * local);
        }
        static const int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3},
                                         {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for (const auto& edge : edges)
            push_line(corner[edge[0]], corner[edge[1]]);
    }

    void Gizmos::ring(const Vec3& center, const Vec3& axis, float radius)
    {
        glm::vec3 u;
        glm::vec3 v;
        perpendicular_basis(glm::normalize(axis), u, v);
        constexpr auto SEGMENTS = 32;
        auto previous = center + (radius * u);
        for (auto i = 1; i <= SEGMENTS; ++i)
        {
            const auto a = (static_cast<float>(i) / static_cast<float>(SEGMENTS)) * TWO_PI;
            const auto point = center + (radius * ((std::cos(a) * u) + (std::sin(a) * v)));
            push_line(previous, point);
            previous = point;
        }
    }

    void Gizmos::wire_sphere(const Vec3& center, float radius)
    {
        ring(center, glm::vec3(1.0F, 0.0F, 0.0F), radius);
        ring(center, glm::vec3(0.0F, 1.0F, 0.0F), radius);
        ring(center, glm::vec3(0.0F, 0.0F, 1.0F), radius);
    }

    void Gizmos::wire_capsule(
        const Vec3& center, float radius, float half_height, const Quat& rotation)
    {
        const auto up = glm::vec3(rotation * glm::vec3(0.0F, 1.0F, 0.0F));
        const auto u = glm::vec3(rotation * glm::vec3(1.0F, 0.0F, 0.0F));
        const auto v = glm::vec3(rotation * glm::vec3(0.0F, 0.0F, 1.0F));
        const auto top = center + (up * half_height);
        const auto bottom = center - (up * half_height);

        // Cap rings (perpendicular to the axis) and the four straight side lines.
        ring(top, up, radius);
        ring(bottom, up, radius);
        for (const auto& side : {u, -u, v, -v})
            push_line(top + (side * radius), bottom + (side * radius));

        // Hemisphere arcs over each cap, in the two planes that contain the axis.
        constexpr auto SEGMENTS = 16;
        const auto arc = [&](const glm::vec3& cap, const glm::vec3& plane_axis, float sign) {
            auto previous = cap + (plane_axis * radius);
            for (auto i = 1; i <= SEGMENTS; ++i)
            {
                const auto a = (static_cast<float>(i) / static_cast<float>(SEGMENTS)) * PI;
                const auto point =
                    cap + (radius * ((std::cos(a) * plane_axis) + (sign * std::sin(a) * up)));
                push_line(previous, point);
                previous = point;
            }
        };
        arc(top, u, 1.0F);
        arc(top, v, 1.0F);
        arc(bottom, u, -1.0F);
        arc(bottom, v, -1.0F);
    }

    void Gizmos::arrow(const Vec3& from, const Vec3& to)
    {
        push_line(from, to);
        const auto delta = to - from;
        const auto length = glm::length(delta);
        if (length < 1e-5F)
            return;
        const auto dir = delta / length;
        glm::vec3 u;
        glm::vec3 v;
        perpendicular_basis(dir, u, v);
        const auto head = length * 0.18F;
        const auto base = to - (dir * head);
        push_line(to, base + (u * head * 0.5F));
        push_line(to, base - (u * head * 0.5F));
        push_line(to, base + (v * head * 0.5F));
        push_line(to, base - (v * head * 0.5F));
    }

    void Gizmos::solid_arrow(const Vec3& from, const Vec3& to, const Color& color, float shaft_radius)
    {
        const auto delta = to - from;
        const auto length = glm::length(delta);
        if (length < 1e-5F)
            return;

        const auto dir = delta / length;
        glm::vec3 u;
        glm::vec3 v;
        perpendicular_basis(dir, u, v);

        const auto head_length = std::min(length * 0.35F, shaft_radius * 9.0F);
        const auto shaft_end = to - (dir * head_length);
        const auto r = shaft_radius;

        const auto corner = [&](const glm::vec3& base, float su, float sv) {
            return base + (u * (su * r)) + (v * (sv * r));
        };
        const auto quad = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                              const glm::vec3& p3) {
            push_triangle(p0, p1, p2, color);
            push_triangle(p0, p2, p3, color);
        };

        // Square-section shaft (four side quads).
        const auto a0 = corner(from, -1, -1);
        const auto a1 = corner(from, 1, -1);
        const auto a2 = corner(from, 1, 1);
        const auto a3 = corner(from, -1, 1);
        const auto b0 = corner(shaft_end, -1, -1);
        const auto b1 = corner(shaft_end, 1, -1);
        const auto b2 = corner(shaft_end, 1, 1);
        const auto b3 = corner(shaft_end, -1, 1);
        quad(a0, a1, b1, b0);
        quad(a1, a2, b2, b1);
        quad(a2, a3, b3, b2);
        quad(a3, a0, b0, b3);

        // Pyramid head: a square base around shaft_end, apex at `to`.
        const auto hr = r * 2.5F;
        const auto base = [&](float su, float sv) {
            return shaft_end + (u * (su * hr)) + (v * (sv * hr));
        };
        const auto h0 = base(-1, -1);
        const auto h1 = base(1, -1);
        const auto h2 = base(1, 1);
        const auto h3 = base(-1, 1);
        push_triangle(h0, h1, to, color);
        push_triangle(h1, h2, to, color);
        push_triangle(h2, h3, to, color);
        push_triangle(h3, h0, to, color);
        push_triangle(h0, h2, h1, color); // base cap
        push_triangle(h0, h3, h2, color);
    }

    void Gizmos::solid_beam(const Vec3& from, const Vec3& to, const Color& color, float radius)
    {
        const auto delta = to - from;
        const auto length = glm::length(delta);
        if (length < 1e-5F)
            return;

        const auto dir = delta / length;
        glm::vec3 u;
        glm::vec3 v;
        perpendicular_basis(dir, u, v);
        const auto r = radius;

        const auto corner = [&](const glm::vec3& base, float su, float sv) {
            return base + (u * (su * r)) + (v * (sv * r));
        };
        const auto quad = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec3& p2,
                              const glm::vec3& p3) {
            push_triangle(p0, p1, p2, color);
            push_triangle(p0, p2, p3, color);
        };

        const auto a0 = corner(from, -1, -1);
        const auto a1 = corner(from, 1, -1);
        const auto a2 = corner(from, 1, 1);
        const auto a3 = corner(from, -1, 1);
        const auto b0 = corner(to, -1, -1);
        const auto b1 = corner(to, 1, -1);
        const auto b2 = corner(to, 1, 1);
        const auto b3 = corner(to, -1, 1);
        quad(a0, a1, b1, b0);
        quad(a1, a2, b2, b1);
        quad(a2, a3, b3, b2);
        quad(a3, a0, b0, b3);
    }

    void Gizmos::solid_cylinder(const Vec3& from, const Vec3& to, float radius, const Color& color)
    {
        const auto delta = to - from;
        const auto length = glm::length(delta);
        if (length < 1e-5F)
            return;

        const auto dir = delta / length;
        glm::vec3 u;
        glm::vec3 v;
        perpendicular_basis(dir, u, v);

        constexpr auto RADIAL_SEGMENTS = 12;
        // Point on the tube surface for the i-th step around the ring, at the given end centre.
        const auto rim = [&](const glm::vec3& center, int i) {
            const auto a = (static_cast<float>(i) / static_cast<float>(RADIAL_SEGMENTS)) * TWO_PI;
            return center + (radius * ((std::cos(a) * u) + (std::sin(a) * v)));
        };

        for (auto i = 0; i < RADIAL_SEGMENTS; ++i)
        {
            const auto a0 = rim(from, i);
            const auto a1 = rim(from, i + 1);
            const auto b0 = rim(to, i);
            const auto b1 = rim(to, i + 1);
            // Side quad.
            push_triangle(a0, a1, b1, color);
            push_triangle(a0, b1, b0, color);
            // End caps (fans about each centre).
            push_triangle(from, a1, a0, color);
            push_triangle(to, b0, b1, color);
        }
    }

    void Gizmos::solid_cone(const Vec3& base, const Vec3& tip, float radius, const Color& color)
    {
        const auto delta = tip - base;
        const auto length = glm::length(delta);
        if (length < 1e-5F)
            return;

        const auto dir = delta / length;
        glm::vec3 u;
        glm::vec3 v;
        perpendicular_basis(dir, u, v);

        constexpr auto RADIAL_SEGMENTS = 12;
        const auto rim = [&](int i) {
            const auto a = (static_cast<float>(i) / static_cast<float>(RADIAL_SEGMENTS)) * TWO_PI;
            return base + (radius * ((std::cos(a) * u) + (std::sin(a) * v)));
        };

        for (auto i = 0; i < RADIAL_SEGMENTS; ++i)
        {
            const auto c0 = rim(i);
            const auto c1 = rim(i + 1);
            push_triangle(c0, c1, tip, color); // side
            push_triangle(base, c0, c1, color); // base cap
        }
    }

    void Gizmos::solid_torus(
        const Vec3& center,
        const Vec3& axis,
        float ring_radius,
        float tube_radius,
        const Color& color)
    {
        const auto n = glm::normalize(axis);
        glm::vec3 u;
        glm::vec3 v;
        perpendicular_basis(n, u, v);

        constexpr auto MAJOR = 32;
        constexpr auto MINOR = 10;
        // Point on the tube surface for the i-th step around the ring and j-th step around the tube.
        const auto point = [&](int i, int j) {
            const auto a = (static_cast<float>(i) / static_cast<float>(MAJOR)) * TWO_PI;
            const auto out = (std::cos(a) * u) + (std::sin(a) * v); // outward direction in the ring plane
            const auto ring_center = center + (ring_radius * out);
            const auto b = (static_cast<float>(j) / static_cast<float>(MINOR)) * TWO_PI;
            return ring_center + (tube_radius * ((std::cos(b) * out) + (std::sin(b) * n)));
        };

        for (auto i = 0; i < MAJOR; ++i)
            for (auto j = 0; j < MINOR; ++j)
            {
                const auto p00 = point(i, j);
                const auto p10 = point(i + 1, j);
                const auto p11 = point(i + 1, j + 1);
                const auto p01 = point(i, j + 1);
                push_triangle(p00, p10, p11, color);
                push_triangle(p00, p11, p01, color);
            }
    }

    void Gizmos::filled_arc(
        const Vec3& center,
        const Vec3& axis,
        float radius,
        const Vec3& start_direction,
        float sweep_radians,
        const Color& color)
    {
        const auto n = glm::normalize(axis);
        // In-plane basis: u along the (projected) start direction, v 90 degrees around the axis.
        auto u = start_direction - (n * glm::dot(start_direction, n));
        if (glm::length(u) < 1e-5F)
        {
            glm::vec3 fallback;
            perpendicular_basis(n, u, fallback);
        }
        u = glm::normalize(u);
        const auto v = glm::normalize(glm::cross(n, u));

        constexpr auto MAX_STEP = TWO_PI / 48.0F;
        const auto segments = std::max(1, static_cast<int>(std::ceil(std::abs(sweep_radians) / MAX_STEP)));
        const auto step = sweep_radians / static_cast<float>(segments);
        auto previous = center + (radius * u);
        for (auto i = 1; i <= segments; ++i)
        {
            const auto a = step * static_cast<float>(i);
            const auto point = center + (radius * ((std::cos(a) * u) + (std::sin(a) * v)));
            push_triangle(center, previous, point, color);
            previous = point;
        }
    }

    void Gizmos::wire_square(const Vec3& center, const Vec2& size, const Quat& rotation)
    {
        const auto hx = size.x * 0.5F;
        const auto hy = size.y * 0.5F;
        const auto c0 = center + (rotation * glm::vec3(-hx, -hy, 0.0F));
        const auto c1 = center + (rotation * glm::vec3(hx, -hy, 0.0F));
        const auto c2 = center + (rotation * glm::vec3(hx, hy, 0.0F));
        const auto c3 = center + (rotation * glm::vec3(-hx, hy, 0.0F));
        push_line(c0, c1);
        push_line(c1, c2);
        push_line(c2, c3);
        push_line(c3, c0);
    }

    void Gizmos::wire_plane(const Vec3& center, const Vec3& normal, float size)
    {
        const auto n = glm::normalize(normal);
        glm::vec3 u;
        glm::vec3 v;
        perpendicular_basis(n, u, v);
        const auto half = size * 0.5F;
        const auto c0 = center + (u * half) + (v * half);
        const auto c1 = center + (u * half) - (v * half);
        const auto c2 = center - (u * half) - (v * half);
        const auto c3 = center - (u * half) + (v * half);
        push_line(c0, c1);
        push_line(c1, c2);
        push_line(c2, c3);
        push_line(c3, c0);
        push_line(center, center + (n * half)); // normal stub
    }

    void Gizmos::axes(const Vec3& origin, const Quat& rotation, float size)
    {
        push_line(origin, origin + (rotation * glm::vec3(size, 0.0F, 0.0F)), Color(0.92F, 0.26F, 0.26F, 1.0F));
        push_line(origin, origin + (rotation * glm::vec3(0.0F, size, 0.0F)), Color(0.35F, 0.85F, 0.35F, 1.0F));
        push_line(origin, origin + (rotation * glm::vec3(0.0F, 0.0F, size)), Color(0.30F, 0.50F, 0.96F, 1.0F));
    }

    void Gizmos::solid_box(const Vec3& center, const Vec3& size, const Quat& rotation)
    {
        const auto half = size * 0.5F;
        glm::vec3 corner[8];
        for (auto i = 0; i < 8; ++i)
        {
            const auto local = glm::vec3(
                (i & 1) ? half.x : -half.x, (i & 2) ? half.y : -half.y, (i & 4) ? half.z : -half.z);
            corner[i] = center + (rotation * local);
        }
        // Six faces, two triangles each (corner indexing matches wire_box's bit layout).
        static const int faces[6][4] = {{0, 2, 6, 4}, {1, 5, 7, 3}, {0, 4, 5, 1},
                                        {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 6, 7, 5}};
        for (const auto& face : faces)
        {
            push_triangle(corner[face[0]], corner[face[1]], corner[face[2]]);
            push_triangle(corner[face[0]], corner[face[2]], corner[face[3]]);
        }
    }

    void Gizmos::solid_square(const Vec3& center, const Vec2& size, const Quat& rotation)
    {
        const auto hx = size.x * 0.5F;
        const auto hy = size.y * 0.5F;
        const auto c0 = center + (rotation * glm::vec3(-hx, -hy, 0.0F));
        const auto c1 = center + (rotation * glm::vec3(hx, -hy, 0.0F));
        const auto c2 = center + (rotation * glm::vec3(hx, hy, 0.0F));
        const auto c3 = center + (rotation * glm::vec3(-hx, hy, 0.0F));
        push_triangle(c0, c1, c2);
        push_triangle(c0, c2, c3);
    }

    void Gizmos::solid_plane(const Vec3& center, const Vec3& normal, float size)
    {
        const auto n = glm::normalize(normal);
        glm::vec3 u;
        glm::vec3 v;
        perpendicular_basis(n, u, v);
        const auto half = size * 0.5F;
        const auto c0 = center + (u * half) + (v * half);
        const auto c1 = center + (u * half) - (v * half);
        const auto c2 = center - (u * half) - (v * half);
        const auto c3 = center - (u * half) + (v * half);
        push_triangle(c0, c1, c2);
        push_triangle(c0, c2, c3);
    }

    void Gizmos::solid_sphere(const Vec3& center, float radius)
    {
        constexpr auto STACKS = 8;
        constexpr auto SLICES = 12;
        const auto at = [&](int stack, int slice) {
            const auto phi = (static_cast<float>(stack) / static_cast<float>(STACKS)) * PI;
            const auto theta =
                (static_cast<float>(slice) / static_cast<float>(SLICES)) * TWO_PI;
            return center
                   + (radius
                      * glm::vec3(
                          std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)));
        };
        for (auto stack = 0; stack < STACKS; ++stack)
            for (auto slice = 0; slice < SLICES; ++slice)
            {
                const auto a = at(stack, slice);
                const auto b = at(stack + 1, slice);
                const auto c = at(stack + 1, slice + 1);
                const auto d = at(stack, slice + 1);
                push_triangle(a, b, c);
                push_triangle(a, c, d);
            }
    }

    void Gizmos::set_external(std::vector<GizmoVertex> lines, std::vector<GizmoVertex> triangles)
    {
        auto lock = std::lock_guard(_mutex);
        _external_lines = std::move(lines);
        _external_triangles = std::move(triangles);
    }

    bool Gizmos::ensure_pipelines(IGraphicsBackend& backend)
    {
        if (_line_pipeline != INVALID_GPU_ID && _triangle_pipeline != INVALID_GPU_ID)
            return true;
        if (_pipeline_failed)
            return false;

        const auto assets = _assets.lock();
        if (!assets)
            return false;

        // The gizmo material is the data-driven source of truth: it names the shader program and the
        // render config; the line + solid pipelines are built from it (the cache's add_pipeline can't
        // express a LINES topology, so the pipelines are built here from the material's shaders).
        const auto material = assets->load<Material>(Handle("Materials/Gizmos/Gizmo.mat"));
        if (!material)
        {
            _pipeline_failed = true;
            TBX_TRACE_ERROR("Gizmos: failed to load the gizmo material.");
            return false;
        }

        const auto vertex = assets->load<Shader>(material->shader.vertex);
        const auto fragment = assets->load<Shader>(material->shader.fragment);
        if (!vertex || !fragment)
        {
            _pipeline_failed = true;
            TBX_TRACE_ERROR("Gizmos: failed to load the gizmo shaders.");
            return false;
        }

        const auto& config = material->config;
        const auto build = [&](PrimitiveType topology, GpuId& out_pipeline) -> bool {
            auto desc = RasterPipelineDesc {};
            desc.shaders = {*vertex, *fragment};
            desc.primitive_type = topology;
            desc.is_depth_test_enabled = config.is_depth_test_enabled;
            desc.is_depth_write_enabled = config.is_depth_write_enabled;
            desc.is_blending_enabled = config.blend_mode != MaterialBlendMode::OPAQUE;
            desc.is_culling_enabled = config.is_cullable;
            desc.debug_name = "Gizmo";
            if (auto result = backend.create_raster_pipeline(desc, out_pipeline); !result)
            {
                TBX_TRACE_ERROR("Gizmos: failed to build a gizmo pipeline: {}", result.get_report());
                return false;
            }
            return true;
        };

        if (!build(PrimitiveType::LINES, _line_pipeline)
            || !build(PrimitiveType::TRIANGLES, _triangle_pipeline))
        {
            _pipeline_failed = true;
            return false;
        }

        return true;
    }

    void Gizmos::draw_list(
        IGraphicsBackend& backend,
        const Mat4& view_projection,
        const std::vector<GizmoVertex>& vertices,
        GpuId pipeline)
    {
        if (vertices.empty() || pipeline == INVALID_GPU_ID)
            return;

        const auto byte_size = static_cast<uint64>(vertices.size() * sizeof(GizmoVertex));
        auto vertex_buffer = INVALID_GPU_ID;
        if (!backend.create_buffer(
                BufferDesc {.usage = BufferUsage::STORAGE, .size = byte_size, .is_dynamic = true},
                vertex_buffer))
            return;
        if (!backend.write_buffer(vertex_buffer, BufferRegion {.size = byte_size}, vertices.data()))
        {
            backend.destroy_resource(vertex_buffer);
            return;
        }

        auto uniform_buffer = INVALID_GPU_ID;
        if (!backend.create_buffer(
                BufferDesc {.usage = BufferUsage::UNIFORM, .size = sizeof(Mat4), .is_dynamic = true},
                uniform_buffer))
        {
            backend.destroy_resource(vertex_buffer);
            return;
        }
        backend.write_buffer(
            uniform_buffer, BufferRegion {.size = sizeof(Mat4)}, &view_projection);

        auto group = INVALID_GPU_ID;
        const auto group_desc = BindGroupDesc {
            .bindings = {ResourceBinding {.binding_slot = 0U, .resource_handle = vertex_buffer},
                         ResourceBinding {.binding_slot = 0U, .resource_handle = uniform_buffer}},
            .debug_name = "Gizmo"};
        if (!backend.create_bind_group(group_desc, group))
        {
            backend.destroy_resource(uniform_buffer);
            backend.destroy_resource(vertex_buffer);
            return;
        }

        // Empty color targets => the active frame output; no clear, so gizmos composite over the scene.
        auto pass = RenderPassDesc {};
        pass.clear_flags = ClearFlags::NONE;
        pass.debug_name = "GizmoOverlay";
        backend.begin_render_pass(pass);
        backend.bind_raster_pipeline(pipeline);
        backend.bind_group(0U, group);
        backend.draw(static_cast<uint32>(vertices.size()), 1U, 0U, 0, 0U);
        backend.end_render_pass();

        backend.destroy_resource(group);
        backend.destroy_resource(uniform_buffer);
        backend.destroy_resource(vertex_buffer);
    }

    void Gizmos::render(IGraphicsBackend& backend, const Mat4& view_projection)
    {
        std::vector<GizmoVertex> lines;
        std::vector<GizmoVertex> triangles;
        {
            auto lock = std::lock_guard(_mutex);
            lines = _lines;
            lines.insert(lines.end(), _external_lines.begin(), _external_lines.end());
            triangles = _triangles;
            triangles.insert(triangles.end(), _external_triangles.begin(), _external_triangles.end());
        }

        if (lines.empty() && triangles.empty())
            return;
        if (!ensure_pipelines(backend))
            return;

        // Triangles first, then lines on top.
        draw_list(backend, view_projection, triangles, _triangle_pipeline);
        draw_list(backend, view_projection, lines, _line_pipeline);
    }
}
