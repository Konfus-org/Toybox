#include "pch.h"
#include "tbx/file_system/json.h"
#include <string_view>

namespace tbx::tests::file_system
{
    TEST(JsonTests, ParsesFromString)
    {
        const String text = "{\n  \"value\": 5,\n  // comment should be ignored\n  \"flag\": true\n}";

        Json json(text);

        int value = {};
        bool flag = {};

        EXPECT_TRUE(json.try_get_int("value", value));
        EXPECT_TRUE(json.try_get_bool("flag", flag));

        EXPECT_EQ(value, 5);
        EXPECT_TRUE(flag);
    }

    TEST(JsonTests, SerializesAccessedValues)
    {
        const String text = "{\n  \"name\": \"temp\"\n}";

        Json json(text);

        String name;

        EXPECT_TRUE(json.try_get_string("name", name));
        EXPECT_EQ(name, "temp");

        const String serialized = json.to_string(2);
        EXPECT_NE(std::string_view(serialized).find("\"name\""), std::string_view::npos);
    }

    TEST(JsonTests, ReadsMixedPrimitiveArrays)
    {
        const String text = "{\n"
                            "  \"ints\": [1, 2, \"skip\"],\n"
                            "  \"bools\": [true, false, 5],\n"
                            "  \"floats\": [1.5, 4, \"nope\"]\n"
                            "}";

        Json json(text);

        List<int> ints;
        List<bool> bools;
        List<double> floats;

        EXPECT_TRUE(json.try_get_ints("ints", ints));
        EXPECT_TRUE(json.try_get_bools("bools", bools));
        EXPECT_TRUE(json.try_get_floats("floats", floats));

        ASSERT_EQ(ints.size(), 2u);
        EXPECT_EQ(ints[0], 1);
        EXPECT_EQ(ints[1], 2);

        ASSERT_EQ(bools.size(), 2u);
        EXPECT_TRUE(bools[0]);
        EXPECT_FALSE(bools[1]);

        ASSERT_EQ(floats.size(), 2u);
        EXPECT_DOUBLE_EQ(floats[0], 1.5);
        EXPECT_DOUBLE_EQ(floats[1], 4.0);
    }

    TEST(JsonTests, ReadsNestedObjects)
    {
        const String text = "{\n"
                            "  \"child\": { \"value\": 7 },\n"
                            "  \"children\": [ { \"name\": \"first\" }, 3, { \"name\": \"second\" } ],\n"
                            "  \"pi\": 3.14\n"
                            "}";

        Json json(text);

        Json child;
        EXPECT_TRUE(json.try_get_child("child", child));

        int value = {};
        EXPECT_TRUE(child.try_get_int("value", value));
        EXPECT_EQ(value, 7);

        double pi = {};
        EXPECT_TRUE(json.try_get_float("pi", pi));
        EXPECT_DOUBLE_EQ(pi, 3.14);

        List<Json> children;
        EXPECT_TRUE(json.try_get_children("children", children));
        ASSERT_EQ(children.size(), 2u);

        String name;
        EXPECT_TRUE(children[0].try_get_string("name", name));
        EXPECT_EQ(name, "first");
        EXPECT_TRUE(children[1].try_get_string("name", name));
        EXPECT_EQ(name, "second");
    }

    TEST(JsonTests, ReadsStringArrays)
    {
        const String text = "{\n  \"values\": [\"first\", 3, \"second\"]\n}";

        Json json(text);

        List<String> values;

        EXPECT_TRUE(json.try_get_strings("values", values));
        ASSERT_EQ(values.size(), 2u);
        EXPECT_EQ(values[0], "first");
        EXPECT_EQ(values[1], "second");
    }
}
