#include "tbx/systems/async/job_system.h"
#include "tbx/systems/async/parallel_for.h"
#include <atomic>
#include <numeric>
#include <vector>

namespace
{
    // A range large enough to cross PARALLEL_FOR_MIN_ITEMS so the worker path is actually taken.
    constexpr size LARGE_COUNT = 4096U;
}

TEST(ParallelForTests, RunsEveryIndexExactlyOnceInParallel)
{
    auto jobs = tbx::JobSystem(4U);
    auto counts = std::vector<std::atomic<int>>(LARGE_COUNT);

    tbx::parallel_for(
        &jobs,
        LARGE_COUNT,
        [&counts](size index) { counts[index].fetch_add(1, std::memory_order_relaxed); });

    for (size i = 0U; i < LARGE_COUNT; ++i)
        EXPECT_EQ(counts[i].load(std::memory_order_relaxed), 1) << "index " << i;
}

TEST(ParallelForTests, WritesDistinctSlotsWithoutContention)
{
    auto jobs = tbx::JobSystem(4U);
    auto values = std::vector<size>(LARGE_COUNT, 0U);

    // Each index writes only its own slot, so the result must match a plain serial fill.
    tbx::parallel_for(
        &jobs,
        LARGE_COUNT,
        [&values](size index) { values[index] = index * 2U; });

    for (size i = 0U; i < LARGE_COUNT; ++i)
        EXPECT_EQ(values[i], i * 2U);
}

TEST(ParallelForTests, RunsInlineWhenNoJobSystem)
{
    auto values = std::vector<int>(64U, 0);

    // A null job system must still process the whole range, on the calling thread.
    tbx::parallel_for(nullptr, values.size(), [&values](size index) { values[index] = 7; });

    EXPECT_EQ(std::accumulate(values.begin(), values.end(), 0), 64 * 7);
}

TEST(ParallelForTests, SmallRangesStillCoverEveryIndex)
{
    auto jobs = tbx::JobSystem(4U);
    // Below PARALLEL_FOR_MIN_ITEMS this runs inline, but it must still touch every index.
    const size small_count = tbx::PARALLEL_FOR_MIN_ITEMS / 2U;
    auto values = std::vector<int>(small_count, 0);

    tbx::parallel_for(&jobs, small_count, [&values](size index) { values[index] = 1; });

    EXPECT_EQ(std::accumulate(values.begin(), values.end(), 0), static_cast<int>(small_count));
}

TEST(ParallelForTests, ZeroCountIsANoOp)
{
    auto jobs = tbx::JobSystem(4U);
    bool called = false;
    tbx::parallel_for(&jobs, 0U, [&called](size) { called = true; });
    EXPECT_FALSE(called);
}
