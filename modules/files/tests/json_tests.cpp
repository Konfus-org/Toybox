#include "pch.h"
#include "tbx/files/json.h"
#include <string>
#include <vector>

namespace tbx::tests::file_system
{
    TEST(JsonTests, ParsesFromString)
    {
        const std::string text =
            "{\n  \"value\": 5,\n  // comment should be ignored\n  \"flag\": true\n}";

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
        const std::string text = "{\n  \"name\": \"temp\"\n}";

        Json json(text);

        std::string name;

        EXPECT_TRUE(json.try_get_string("name", name));
        EXPECT_EQ(name, "temp");

        const std::string serialized = json.to_string(2);
        EXPECT_NE(serialized.find("\"name\""), std::string::npos);
    }

    TEST(JsonTests, ReadsMixedPrimitiveArrays)
    {
        const std::string text = "{\n"
                                 "  \"ints\": [1, 2, \"skip\"],\n"
                                 "  \"bools\": [true, false, 5],\n"
                                 "  \"floats\": [1.5, 4, \"nope\"]\n"
                                 "}";

        Json json(text);

        std::vector<int> ints;
        std::vector<bool> bools;
        std::vector<float> floats;

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
        EXPECT_FLOAT_EQ(floats[0], 1.5);
        EXPECT_FLOAT_EQ(floats[1], 4.0);
    }

    TEST(JsonTests, ReadsStrictFloatArrays)
    {
        const std::string text = "{\n"
                                 "  \"values\": [1.5, 4, 2.25],\n"
                                 "  \"mixed\": [1, \"nope\"],\n"
                                 "  \"empty\": []\n"
                                 "}";

        Json json(text);

        std::vector<float> values;
        EXPECT_TRUE(json.try_get_floats("values", values));
        ASSERT_EQ(values.size(), 3u);
        EXPECT_FLOAT_EQ(values[0], 1.5);
        EXPECT_FLOAT_EQ(values[1], 4.0);
        EXPECT_FLOAT_EQ(values[2], 2.25);

        std::vector<float> sized;
        EXPECT_TRUE(json.try_get_floats("values", 3u, sized));
        ASSERT_EQ(sized.size(), 3u);

        std::vector<float> mismatch;
        EXPECT_FALSE(json.try_get_floats("values", 2u, mismatch));

        std::vector<float> mixed;
        EXPECT_FALSE(json.try_get_floats("mixed", 2u, mixed));

        std::vector<float> empty;
        EXPECT_FALSE(json.try_get_floats("empty", empty));
    }

    TEST(JsonTests, ReadsNestedObjects)
    {
        const std::string text =
            "{\n"
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

        float pi = {};
        EXPECT_TRUE(json.try_get_float("pi", pi));
        EXPECT_FLOAT_EQ(pi, 3.14);

        std::vector<Json> children;
        EXPECT_TRUE(json.try_get_children("children", children));
        ASSERT_EQ(children.size(), 2u);

        std::string name;
        EXPECT_TRUE(children[0].try_get_string("name", name));
        EXPECT_EQ(name, "first");
        EXPECT_TRUE(children[1].try_get_string("name", name));
        EXPECT_EQ(name, "second");
    }

    TEST(JsonTests, ReadsStringArrays)
    {
        const std::string text = "{\n  \"values\": [\"first\", 3, \"second\"]\n}";

        Json json(text);

        std::vector<std::string> values;

        EXPECT_TRUE(json.try_get_strings("values", values));
        ASSERT_EQ(values.size(), 2u);
        EXPECT_EQ(values[0], "first");
        EXPECT_EQ(values[1], "second");
    }
}

