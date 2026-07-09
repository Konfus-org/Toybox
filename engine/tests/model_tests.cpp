#include "tbx/types/assets/model.h"
#include "tbx/types/components/mesh.h"
#include <limits>

// A translation-only world matrix (keeps the tests free of a matrix-transform dependency).
static tbx::Mat4 translation(float x, float y, float z)
{
    auto matrix = tbx::Mat4(1.0F);
    matrix[3] = tbx::Vec4(x, y, z, 1.0F);
    return matrix;
}

// A two-part model: the same cube mesh drawn at two spots along -Z (near at -5, far at -10).
// (Every Model constructor authors one identity part; replace it so the placement is explicit.)
static tbx::Model two_part_cube_model()
{
    auto model = tbx::Model(tbx::Mesh::CUBE);
    model.parts = {
        tbx::ModelPart {.transform = translation(0.0F, 0.0F, -5.0F)},
        tbx::ModelPart {.transform = translation(0.0F, 0.0F, -10.0F)},
    };
    return model;
}

// Positive: the nearest part's front face wins, and the distance is in world units.
TEST(ModelGeometryTests, RayIntersectsModelHitsNearestPart)
{
    const auto model = two_part_cube_model();
    ASSERT_TRUE(tbx::Mesh::CUBE.bounds.is_valid);
    const auto front_z = tbx::Mesh::CUBE.bounds.maximum.z;

    const auto ray = tbx::Ray {
        .origin = tbx::Vec3(0.0F),
        .direction = tbx::Vec3(0.0F, 0.0F, -1.0F),
    };
    auto distance = 0.0F;
    ASSERT_TRUE(tbx::ray_intersects_model(ray, model, tbx::Mat4(1.0F), distance));
    EXPECT_NEAR(distance, 5.0F - front_z, 1e-3F);
}

// Negative: a ray pointing away from every part misses.
TEST(ModelGeometryTests, RayIntersectsModelMissesBehind)
{
    const auto model = two_part_cube_model();

    const auto ray = tbx::Ray {
        .origin = tbx::Vec3(0.0F),
        .direction = tbx::Vec3(0.0F, 0.0F, 1.0F),
    };
    auto distance = 0.0F;
    EXPECT_FALSE(tbx::ray_intersects_model(ray, model, tbx::Mat4(1.0F), distance));
}

// Positive: the expanded AABB is the union of every part's transformed mesh bounds, and a part
// with an out-of-range mesh index is skipped rather than crashing.
TEST(ModelGeometryTests, ExpandAabbWithModelUnionsPartBounds)
{
    auto model = two_part_cube_model();
    model.parts.push_back(tbx::ModelPart {.mesh_index = 7U}); // dangling index: ignored
    const auto lo = tbx::Mesh::CUBE.bounds.minimum;
    const auto hi = tbx::Mesh::CUBE.bounds.maximum;

    auto minimum = tbx::Vec3(std::numeric_limits<float>::max());
    auto maximum = tbx::Vec3(std::numeric_limits<float>::lowest());
    ASSERT_TRUE(tbx::expand_aabb_with_model(model, tbx::Mat4(1.0F), minimum, maximum));

    EXPECT_NEAR(minimum.z, -10.0F + lo.z, 1e-4F);
    EXPECT_NEAR(maximum.z, -5.0F + hi.z, 1e-4F);
    EXPECT_NEAR(minimum.x, lo.x, 1e-4F);
    EXPECT_NEAR(maximum.x, hi.x, 1e-4F);
    EXPECT_NEAR(minimum.y, lo.y, 1e-4F);
    EXPECT_NEAR(maximum.y, hi.y, 1e-4F);
}

// Negative: a model with no meshes contributes nothing.
TEST(ModelGeometryTests, ExpandAabbWithModelReportsNoBounds)
{
    auto model = tbx::Model();
    model.meshes.clear();
    model.parts.clear();

    auto minimum = tbx::Vec3(std::numeric_limits<float>::max());
    auto maximum = tbx::Vec3(std::numeric_limits<float>::lowest());
    EXPECT_FALSE(tbx::expand_aabb_with_model(model, tbx::Mat4(1.0F), minimum, maximum));
}

// Positive: without parts every mesh visits at the root matrix; with parts only valid part
// indices visit, each under root * part.transform.
TEST(ModelGeometryTests, ForEachModelMeshFollowsThePartsRule)
{
    auto model = tbx::Model(tbx::Mesh::CUBE);
    model.parts.clear(); // no parts: every mesh visits at the root
    auto visits = 0;
    tbx::for_each_model_mesh(
        model,
        tbx::Mat4(1.0F),
        [&visits](const tbx::Mesh&, const tbx::Mat4&) { ++visits; });
    EXPECT_EQ(visits, 1);

    model.parts = {
        tbx::ModelPart {.transform = translation(1.0F, 0.0F, 0.0F)},
        tbx::ModelPart {.mesh_index = 7U}, // dangling index: skipped
    };
    auto part_visits = 0;
    auto visited_x = 0.0F;
    tbx::for_each_model_mesh(
        model,
        tbx::Mat4(1.0F),
        [&part_visits, &visited_x](const tbx::Mesh&, const tbx::Mat4& matrix)
        {
            ++part_visits;
            visited_x = matrix[3].x;
        });
    EXPECT_EQ(part_visits, 1);
    EXPECT_NEAR(visited_x, 1.0F, 1e-6F);
}
