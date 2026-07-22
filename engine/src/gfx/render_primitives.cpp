#include "render_primitives.h"
#include <cmath>

namespace tbx
{
    //// HELPERS ////

    static void push_vertex(
        std::vector<float>& vertices,
        const Vec3& position,
        const Vec3& normal,
        const Vec2& uv)
    {
        for (const float value :
             {position.x, position.y, position.z, normal.x, normal.y, normal.z, uv.x, uv.y})
            vertices.push_back(value);
    }

    static void push_box(std::vector<float>& vertices, const Vec3& center, const Vec3& half)
    {
        const Vec3 normals[6] =
            {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
        for (const Vec3& normal : normals)
        {
            // Build a face basis from the normal; corners wind counter-clockwise. The face
            // axes are axis-aligned, so scaling them component-wise by the half extents
            // shapes the box.
            const Vec3 up = std::abs(normal.y) > 0.5f ? Vec3(0, 0, -normal.y) : Vec3(0, 1, 0);
            const Vec3 right = Vec3(
                up.y * normal.z - up.z * normal.y,
                up.z * normal.x - up.x * normal.z,
                up.x * normal.y - up.y * normal.x);
            const Vec3 face =
                center + Vec3(normal.x * half.x, normal.y * half.y, normal.z * half.z);
            const Vec3 r = Vec3(right.x * half.x, right.y * half.y, right.z * half.z);
            const Vec3 u = Vec3(up.x * half.x, up.y * half.y, up.z * half.z);
            const Vec3 corners[4] = {face - r - u, face + r - u, face + r + u, face - r + u};
            const Vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
            for (const int index : {0, 1, 2, 0, 2, 3})
                push_vertex(vertices, corners[index], normal, uvs[index]);
        }
    }

    //// BUILDERS ////

    std::vector<float> build_cube_vertices()
    {
        auto vertices = std::vector<float>();
        push_box(vertices, Vec3(0.0f), Vec3(0.5f, 0.5f, 0.5f));
        return vertices;
    }

    std::vector<float> build_fullscreen_vertices()
    {
        // One triangle covering NDC; uv doubles past 1 so v_uv spans [0,1] over the viewport.
        auto vertices = std::vector<float>();
        const Vec3 forward = Vec3(0.0f, 0.0f, 1.0f);
        push_vertex(vertices, Vec3(-1.0f, -1.0f, 0.0f), forward, Vec2(0.0f, 0.0f));
        push_vertex(vertices, Vec3(3.0f, -1.0f, 0.0f), forward, Vec2(2.0f, 0.0f));
        push_vertex(vertices, Vec3(-1.0f, 3.0f, 0.0f), forward, Vec2(0.0f, 2.0f));
        return vertices;
    }

    std::vector<float> build_plane_vertices()
    {
        auto vertices = std::vector<float>();
        const Vec3 up = Vec3(0.0f, 1.0f, 0.0f);
        const Vec3 corners[4] =
            {{-0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, 0.5f}, {0.5f, 0.0f, -0.5f}, {-0.5f, 0.0f, -0.5f}};
        const Vec2 uvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
        for (const int index : {0, 1, 2, 0, 2, 3})
            push_vertex(vertices, corners[index], up, uvs[index]);
        return vertices;
    }

    std::vector<float> build_question_mark_vertices()
    {
        // A blocky question mark built from boxes — the unmistakable missing-mesh stand-in
        // (docs/RenderFailures.md), roughly filling the unit-cube footprint.
        auto vertices = std::vector<float>();
        constexpr float DEPTH = 0.07f;
        push_box(vertices, Vec3(0.0f, 0.42f, 0.0f), Vec3(0.22f, 0.07f, DEPTH)); // top of the arc
        push_box(vertices, Vec3(-0.22f, 0.31f, 0.0f), Vec3(0.07f, 0.09f, DEPTH)); // arc start
        push_box(vertices, Vec3(0.22f, 0.25f, 0.0f), Vec3(0.07f, 0.13f, DEPTH)); // arc right side
        push_box(vertices, Vec3(0.07f, 0.12f, 0.0f), Vec3(0.16f, 0.06f, DEPTH)); // hook inward
        push_box(vertices, Vec3(0.0f, -0.06f, 0.0f), Vec3(0.07f, 0.13f, DEPTH)); // stem
        push_box(vertices, Vec3(0.0f, -0.40f, 0.0f), Vec3(0.09f, 0.09f, DEPTH)); // the dot
        return vertices;
    }

    std::vector<float> build_sphere_vertices(const int rings, const int segments)
    {
        auto vertices = std::vector<float>();
        auto point = [&](const int ring, const int segment)
        {
            const float phi = 3.14159265f * static_cast<float>(ring) / rings;
            const float theta = 2.0f * 3.14159265f * static_cast<float>(segment) / segments;
            const Vec3 normal = Vec3(
                std::sin(phi) * std::cos(theta),
                std::cos(phi),
                std::sin(phi) * std::sin(theta));
            const Vec2 uv =
                Vec2(static_cast<float>(segment) / segments, static_cast<float>(ring) / rings);
            push_vertex(vertices, normal * 0.5f, normal, uv);
        };
        for (int ring = 0; ring < rings; ++ring)
        {
            for (int segment = 0; segment < segments; ++segment)
            {
                point(ring, segment);
                point(ring + 1, segment + 1);
                point(ring + 1, segment);
                point(ring, segment);
                point(ring, segment + 1);
                point(ring + 1, segment + 1);
            }
        }
        return vertices;
    }
}
