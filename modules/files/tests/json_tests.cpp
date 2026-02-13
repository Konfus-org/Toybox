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

        EXPECT_TRUE(json.try_get<int>("value", value));
        EXPECT_TRUE(json.try_get<bool>("flag", flag));

        EXPECT_EQ(value, 5);
        EXPECT_TRUE(flag);
    }

    TEST(JsonTests, ReadsStructuredMathTypes)
    {
        const std::string text = "{\n"
                                 "  \"color\": [0.1, 0.2, 0.3, 1.0],\n"
                                 "  \"position\": [2.0, 3.0, 4.0],\n"
                                 "  \"rotation\": [0.0, 0.0, 0.0, 1.0]\n"
                                 "}";

        Json json(text);

        auto color = RgbaColor();
        auto position = Vec3();
        auto rotation = Quat();

        EXPECT_TRUE(json.try_get<RgbaColor>("color", color));
        EXPECT_TRUE(json.try_get<Vec3>("position", position));
        EXPECT_TRUE(json.try_get<Quat>("rotation", rotation));

        EXPECT_FLOAT_EQ(color.r, 0.1f);
        EXPECT_FLOAT_EQ(position.y, 3.0f);
        EXPECT_FLOAT_EQ(rotation.w, 1.0f);
    }

    TEST(JsonTests, ReadsTypedArrays)
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

        EXPECT_TRUE(json.try_get<int>("ints", ints));
        EXPECT_TRUE(json.try_get<bool>("bools", bools));
        EXPECT_TRUE(json.try_get<float>("floats", floats));
        EXPECT_TRUE(json.try_get<float>("floats", 2U, floats));
        EXPECT_FALSE(json.try_get<float>("floats", 3U, floats));

        ASSERT_EQ(ints.size(), 2u);
        ASSERT_EQ(bools.size(), 2u);
        EXPECT_GE(floats.size(), 2u);
    }
}
