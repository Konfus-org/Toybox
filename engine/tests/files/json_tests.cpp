#include "tbx/systems/files/json.h"
#include "tbx/types/color.h"
#include "tbx/types/handle.h"
#include "tbx/types/quaternions.h"
#include "tbx/types/vectors.h"

namespace tbx::tests::file_system
{
    TEST(JsonTests, ParsesFromString)
    {
        const std::string text =
            "{\n  \"value\": 5,\n  // comment should be ignored\n  \"flag\": true\n}";

        Json json = JsonParser::parse(text);

        int value = {};
        bool flag = {};

        EXPECT_TRUE(JsonParser::try_get(json, "value", value));
        EXPECT_TRUE(JsonParser::try_get(json, "flag", flag));

        EXPECT_EQ(value, 5);
        EXPECT_TRUE(flag);
    }

    TEST(JsonTests, ReadsStructuredMathTypes)
    {
        const std::string text =
            "{\n"
            "  \"color\": { \"r\": 0.1, \"g\": 0.2, \"b\": 0.3, \"a\": 1.0 },\n"
            "  \"position\": { \"x\": 2.0, \"y\": 3.0, \"z\": 4.0 },\n"
            "  \"rotation\": { \"x\": 0.0, \"y\": 0.0, \"z\": 0.0, \"w\": 1.0 }\n"
            "}";

        Json json = JsonParser::parse(text);

        auto color = Color();
        auto position = Vec3();
        auto rotation = Quat();

        EXPECT_TRUE(JsonParser::try_get(json, "color", color));
        EXPECT_TRUE(JsonParser::try_get(json, "position", position));
        EXPECT_TRUE(JsonParser::try_get(json, "rotation", rotation));

        EXPECT_FLOAT_EQ(color.r, 0.1f);
        EXPECT_FLOAT_EQ(position.y, 3.0f);
        EXPECT_FLOAT_EQ(rotation.w, 1.0f);
    }

    TEST(JsonTests, ReadsTypedArrays)
    {
        const std::string text =
            "{\n"
            "  \"ints\": [1, 2, \"skip\"],\n"
            "  \"bools\": [true, false, 5],\n"
            "  \"floats\": [1.5, 4, \"nope\"]\n"
            "}";

        Json json = JsonParser::parse(text);

        std::vector<int> ints;
        std::vector<bool> bools;
        std::vector<float> floats;

        EXPECT_TRUE(JsonParser::try_get(json, "ints", ints));
        EXPECT_TRUE(JsonParser::try_get(json, "bools", bools));
        EXPECT_TRUE(JsonParser::try_get(json, "floats", floats));
        EXPECT_TRUE(JsonParser::try_get(json, "floats", 2U, floats));
        EXPECT_FALSE(JsonParser::try_get(json, "floats", 3U, floats));

        ASSERT_EQ(ints.size(), 2u);
        ASSERT_EQ(bools.size(), 2u);
        EXPECT_GE(floats.size(), 2u);
    }

    TEST(JsonTests, ReadsHandleFromTbxSerializableObject)
    {
        // Arrange
        const std::string text =
            "{\n"
            "  \"handle\": {\n"
            "    \"_id\": { \"value\": 77 }\n"
            "  }\n"
            "}";

        auto json = JsonParser::parse(text);
        auto handle = Handle();

        // Act
        const auto was_loaded = JsonParser::try_get(json, "handle", handle);

        // Assert
        EXPECT_TRUE(was_loaded);
        EXPECT_EQ(handle.id, Uuid(77U));
        EXPECT_TRUE(handle.name.empty());
    }
}
