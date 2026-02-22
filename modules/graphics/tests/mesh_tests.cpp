#include "PCH.h"
#include "tbx/graphics/mesh.h"

namespace tbx::tests::graphics
{
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

    // Validates globally available built-in meshes are initialized and non-empty.
    TEST(MeshTests, BuiltInMeshes_AreInitialized)
    {
        // Arrange
        const Mesh& triangle_mesh = triangle;
        const Mesh& quad_mesh = quad;
        const Mesh& cube_mesh_ref = cube_mesh;
        const Mesh& sphere_mesh_ref = sphere_mesh;
        const Mesh& capsule_mesh_ref = capsule_mesh;

        // Act
        const bool triangle_has_data =
            !triangle_mesh.vertices.empty() && !triangle_mesh.indices.empty();
        const bool quad_has_data = !quad_mesh.vertices.empty() && !quad_mesh.indices.empty();
        const bool cube_has_data =
            !cube_mesh_ref.vertices.empty() && !cube_mesh_ref.indices.empty();
        const bool sphere_has_data =
            !sphere_mesh_ref.vertices.empty() && !sphere_mesh_ref.indices.empty();
        const bool capsule_has_data =
            !capsule_mesh_ref.vertices.empty() && !capsule_mesh_ref.indices.empty();

        // Assert
        EXPECT_TRUE(triangle_has_data);
        EXPECT_TRUE(quad_has_data);
        EXPECT_TRUE(cube_has_data);
        EXPECT_TRUE(sphere_has_data);
        EXPECT_TRUE(capsule_has_data);
    }
}
