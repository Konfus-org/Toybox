#include "pch.h"
#include "tbx/common/collections.h"
#include "tbx/common/string.h"
#include <numeric>
#include <string>

namespace tbx::tests::common
{
    TEST(CollectionsTests, LinqTransformsSequences)
    {
        List<int> values = {1, 2, 3, 4};

        auto doubled = values.select([](const int value) { return value * 2; });
        auto evens = values.where([](const int value) { return value % 2 == 0; });

        EXPECT_EQ(doubled.get_count(), 4U);
        EXPECT_EQ(doubled[0], 2);
        EXPECT_EQ(doubled[3], 8);

        EXPECT_EQ(evens.get_count(), 2U);
        EXPECT_EQ(evens[0], 2);
        EXPECT_EQ(evens[1], 4);
    }

    TEST(CollectionsTests, LinqQueriesEvaluateState)
    {
        List<int> values = {3, 5, 7};

        EXPECT_TRUE(values.any());
        EXPECT_TRUE(values.any([](const int value) { return value == 5; }));
        EXPECT_FALSE(values.any([](const int value) { return value % 2 == 0; }));

        EXPECT_TRUE(values.all([](const int value) { return value > 1; }));
        EXPECT_FALSE(values.all([](const int value) { return value == 3; }));

        EXPECT_EQ(values.count(), 3U);
        EXPECT_EQ(values.count([](const int value) { return value > 4; }), 2U);
        EXPECT_TRUE(values.contains(7));
        EXPECT_FALSE(values.contains(9));
    }

    TEST(CollectionsTests, LinqRetrievesExpectedElements)
    {
        List<int> values = {9, 11, 13};

        EXPECT_EQ(values.first(), 9);
        EXPECT_EQ(values.first([](const int value) { return value > 10; }), 11);
        EXPECT_EQ(values.first_or_default(-1), 9);
        EXPECT_EQ(values.first_or_default([](const int value) { return value == 99; }, -1), -1);

        List<int> empty_values;
        EXPECT_THROW(empty_values.first(), std::out_of_range);
        EXPECT_EQ(empty_values.first_or_default(42), 42);
    }

    TEST(CollectionsTests, CollectionsSupportAddEmplaceAndConcatenation)
    {
        HashMap<int, String> map;
        EXPECT_TRUE(map.add(1, "one"));
        EXPECT_FALSE(map.add(1, "duplicate"));
        EXPECT_TRUE(map.emplace(2, "two"));
        EXPECT_EQ(map.get_count(), 2U);
        EXPECT_TRUE(map.remove(1));
        EXPECT_FALSE(map.remove(3));

        HashMap<int, String> other_map;
        other_map.add(3, "three");
        HashMap<int, String> combined_map = map;
        for (const auto& entry : other_map)
        {
            combined_map.add(entry.first, entry.second);
        }

        EXPECT_EQ(combined_map.get_count(), 2U);
        EXPECT_EQ(combined_map[2], "two");
        EXPECT_EQ(combined_map[3], "three");

        HashSet<int> first_set = {1, 2};
        EXPECT_TRUE(first_set.add(3));
        EXPECT_FALSE(first_set.add(3));
        HashSet<int> second_set = {3, 4};
        auto union_set = first_set.union_with(second_set);

        EXPECT_EQ(union_set.get_count(), 4U);
        EXPECT_TRUE(union_set.contains(4));

        List<int> first_list = {1, 2};
        EXPECT_TRUE(first_list.add(3));
        List<int> second_list = {4};
        List<int> combined_list = first_list;
        for (const auto& value : second_list)
        {
            combined_list.add(value);
        }

        EXPECT_EQ(combined_list.get_count(), 4U);
        EXPECT_EQ(combined_list[3], 4);
    }

    TEST(CollectionsTests, ArrayIteratesOverTrackedSize)
    {
        Array<int, 5> values;
        values.add(10);
        values.add(20);
        values.add(30);

        auto sum = std::accumulate(values.begin(), values.end(), 0);

        EXPECT_EQ(sum, 60);
        EXPECT_EQ(values.rbegin()[0], 30);
    }
}
