#include "PCH.h"
#include "tbx/graphics/mesh.h"
#include <array>

namespace tbx::tests::graphics
{
    static bool are_all_triangles_outward_facing(const Mesh& mesh)
    {
        constexpr size_t vertex_stride = 12U;
        const auto& vertices = mesh.vertices.vertices;
        const auto& indices = mesh.indices;
        if ((indices.size() % 3U) != 0U)
            return false;

        auto read_position = [&vertices](uint32 vertex_index)
        {
            const size_t base = static_cast<size_t>(vertex_index) * vertex_stride;
            return std::array<float, 3> {
                vertices[base + 0U],
                vertices[base + 1U],
                vertices[base + 2U],
            };
        };

        for (size_t triangle_index = 0U; triangle_index < indices.size(); triangle_index += 3U)
        {
            const auto a = read_position(indices[triangle_index + 0U]);
            const auto b = read_position(indices[triangle_index + 1U]);
            const auto c = read_position(indices[triangle_index + 2U]);

            const auto ab = std::array<float, 3> {
                b[0] - a[0],
                b[1] - a[1],
                b[2] - a[2],
            };
            const auto ac = std::array<float, 3> {
                c[0] - a[0],
                c[1] - a[1],
                c[2] - a[2],
            };
            const auto face_normal = std::array<float, 3> {
                (ab[1] * ac[2]) - (ab[2] * ac[1]),
                (ab[2] * ac[0]) - (ab[0] * ac[2]),
                (ab[0] * ac[1]) - (ab[1] * ac[0]),
            };
            const auto centroid = std::array<float, 3> {
                (a[0] + b[0] + c[0]) / 3.0F,
                (a[1] + b[1] + c[1]) / 3.0F,
                (a[2] + b[2] + c[2]) / 3.0F,
            };
            const float outward_dot = (face_normal[0] * centroid[0])
                                      + (face_normal[1] * centroid[1])
                                      + (face_normal[2] * centroid[2]);
            if (outward_dot <= 0.0F)
                return false;
        }

        return true;
    }

    // Validates built-in cube mesh topology and buffer population.
    TEST(MeshTests, MakeCube_ReturnsExpectedTopology)
    {
        // Arrange
        Mesh mesh = make_cube();

        // Act
        const size_t vertex_float_count = mesh.vertices.vertices.size();
        const size_t index_count = mesh.indices.size();

        // Assert
        EXPECT_EQ(vertex_float_count, 24U * 12U);
        EXPECT_EQ(index_count, 36U);
    }

    // Validates built-in sphere mesh topology and buffer population.
    TEST(MeshTests, MakeSphere_ReturnsExpectedTopology)
    {
        // Arrange
        Mesh mesh = make_sphere();

        // Act
        const size_t vertex_float_count = mesh.vertices.vertices.size();
        const size_t index_count = mesh.indices.size();

        // Assert
        EXPECT_EQ(vertex_float_count, 425U * 12U);
        EXPECT_EQ(index_count, 2160U);
    }

    // Validates sphere triangles are wound outward for correct backface culling.
    TEST(MeshTests, MakeSphere_TrianglesAreOutwardFacing)
    {
        // Arrange
        Mesh mesh = make_sphere();

        // Act
        const bool is_outward_wound = are_all_triangles_outward_facing(mesh);

        // Assert
        EXPECT_TRUE(is_outward_wound);
    }

    // Validates built-in capsule mesh topology and buffer population.
    TEST(MeshTests, MakeCapsule_ReturnsExpectedTopology)
    {
        // Arrange
        Mesh mesh = make_capsule();

        // Act
        const size_t vertex_float_count = mesh.vertices.vertices.size();
        const size_t index_count = mesh.indices.size();

        // Assert
        EXPECT_EQ(vertex_float_count, 625U * 12U);
        EXPECT_EQ(index_count, 3456U);
    }

    // Validates fullscreen quad mesh topology and clip-space coverage.
    TEST(MeshTests, MakeFullscreenQuad_ReturnsExpectedTopology)
    {
        // Arrange
        Mesh mesh = make_fullscreen_quad();

        // Act
        const size_t vertex_float_count = mesh.vertices.vertices.size();
        const size_t index_count = mesh.indices.size();
        const float first_x = mesh.vertices.vertices[0];
        const float first_y = mesh.vertices.vertices[1];
        const float third_x = mesh.vertices.vertices[24];
        const float third_y = mesh.vertices.vertices[25];

        // Assert
        EXPECT_EQ(vertex_float_count, 4U * 12U);
        EXPECT_EQ(index_count, 6U);
        EXPECT_FLOAT_EQ(first_x, -1.0F);
        EXPECT_FLOAT_EQ(first_y, -1.0F);
        EXPECT_FLOAT_EQ(third_x, 1.0F);
        EXPECT_FLOAT_EQ(third_y, 1.0F);
    }

    // Validates globally available built-in meshes are initialized and non-empty.
    TEST(MeshTests, BuiltInMeshes_AreInitialized)
    {
        // Arrange
        const Mesh& triangle_mesh = triangle;
        const Mesh& quad_mesh = quad;
        const Mesh& fullscreen_quad_mesh = fullscreen_quad;
        const Mesh& cube_mesh_ref = cube;
        const Mesh& sphere_mesh_ref = sphere;
        const Mesh& capsule_mesh_ref = capsule;

        // Act
        const bool triangle_has_data =
            !triangle_mesh.vertices.empty() && !triangle_mesh.indices.empty();
        const bool quad_has_data = !quad_mesh.vertices.empty() && !quad_mesh.indices.empty();
        const bool fullscreen_quad_has_data =
            !fullscreen_quad_mesh.vertices.empty() && !fullscreen_quad_mesh.indices.empty();
        const bool cube_has_data =
            !cube_mesh_ref.vertices.empty() && !cube_mesh_ref.indices.empty();
        const bool sphere_has_data =
            !sphere_mesh_ref.vertices.empty() && !sphere_mesh_ref.indices.empty();
        const bool capsule_has_data =
            !capsule_mesh_ref.vertices.empty() && !capsule_mesh_ref.indices.empty();

        // Assert
        EXPECT_TRUE(triangle_has_data);
        EXPECT_TRUE(quad_has_data);
        EXPECT_TRUE(fullscreen_quad_has_data);
        EXPECT_TRUE(cube_has_data);
        EXPECT_TRUE(sphere_has_data);
        EXPECT_TRUE(capsule_has_data);
    }
}
