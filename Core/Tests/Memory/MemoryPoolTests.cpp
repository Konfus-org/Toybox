#include "PCH.h"
#include "Tbx/Memory/MemoryPool.h"

namespace Tbx::Tests::Memory
{
    namespace
    {
        struct Dummy
        {
            explicit Dummy(int v)
                : Value(v)
            {
            }

            int Value = 0;
        };
    }

    TEST(MemoryPoolTests, Provide_UsesPooledStorage)
    {
        MemoryPool<Dummy> pool(2);

        auto first = pool.Provide(10);
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(first->Value, 10);
        EXPECT_EQ(pool.Count(), 1ull);
        EXPECT_FALSE(pool.IsFull());

        auto second = pool.Provide(20);
        ASSERT_NE(second, nullptr);
        EXPECT_EQ(second->Value, 20);
        EXPECT_EQ(pool.Count(), 2ull);
        EXPECT_TRUE(pool.IsFull());

        first.reset();
        EXPECT_EQ(pool.Count(), 1ull);
        EXPECT_FALSE(pool.IsFull());

        auto third = pool.Provide(30);
        ASSERT_NE(third, nullptr);
        EXPECT_EQ(third->Value, 30);
        EXPECT_EQ(pool.Count(), 2ull);
    }

    TEST(MemoryPoolTests, Reserve_AddsCapacity)
    {
        MemoryPool<Dummy> pool(1);
        EXPECT_EQ(pool.Capacity(), 1ull);

        auto first = pool.Provide(10);
        ASSERT_NE(first, nullptr);
        EXPECT_EQ(first->Value, 10);

        pool.Reserve(2);
        EXPECT_EQ(pool.Capacity(), 3ull);
        EXPECT_FALSE(pool.IsFull());

        auto second = pool.Provide(20);
        auto third = pool.Provide(30);

        ASSERT_NE(second, nullptr);
        ASSERT_NE(third, nullptr);
        EXPECT_EQ(first->Value, 10);
        EXPECT_EQ(second->Value, 20);
        EXPECT_EQ(third->Value, 30);
        EXPECT_EQ(pool.Count(), 3ull);
        EXPECT_TRUE(pool.IsFull());
    }
}
