#include "PCH.h"
#include "Tbx/Stages/Blocks.h"
#include "Tbx/Math/Transform.h"
#include "Tbx/Math/Vectors.h"
#include "Tbx/Graphics/Mesh.h"

namespace Tbx::Tests::Stages
{
    TEST(BlockCollectionTests, AddAndGet_ReturnsStoredBlock)
    {
        BlockCollection blocks;

        auto& transform = blocks.Add<Transform>();
        transform.SetPosition({ 1.0f, 2.0f, 3.0f });

        ASSERT_TRUE(blocks.Contains<Transform>());

        const auto& stored = blocks.Get<Transform>();
        EXPECT_FLOAT_EQ(stored.Position.X, 1.0f);
        EXPECT_FLOAT_EQ(stored.Position.Y, 2.0f);
        EXPECT_FLOAT_EQ(stored.Position.Z, 3.0f);

        const BlockCollection& constBlocks = blocks;
        const auto& constStored = constBlocks.Get<Transform>();
        EXPECT_FLOAT_EQ(constStored.Position.X, 1.0f);
        EXPECT_FLOAT_EQ(constStored.Position.Y, 2.0f);
        EXPECT_FLOAT_EQ(constStored.Position.Z, 3.0f);
    }

    TEST(BlockCollectionTests, Remove_ErasesStoredBlock)
    {
        BlockCollection blocks;

        blocks.Add<Mesh>();
        ASSERT_TRUE(blocks.Contains<Mesh>());

        EXPECT_NO_THROW(blocks.Remove<Mesh>());
        EXPECT_FALSE(blocks.Contains<Mesh>());

        EXPECT_NO_THROW(blocks.Remove<Mesh>());
    }

    TEST(BlockCollectionTests, Add_ReplacesExistingInstance)
    {
        BlockCollection blocks;

        blocks.Add<Transform>().SetPosition({ 1.0f, 0.0f, 0.0f });
        blocks.Add<Transform>().SetPosition({ 2.0f, 0.0f, 0.0f });

        const auto& transform = blocks.Get<Transform>();
        EXPECT_FLOAT_EQ(transform.Position.X, 2.0f);
        EXPECT_FLOAT_EQ(transform.Position.Y, 0.0f);
        EXPECT_FLOAT_EQ(transform.Position.Z, 0.0f);
    }
}
