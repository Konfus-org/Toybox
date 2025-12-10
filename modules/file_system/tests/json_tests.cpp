#include "pch.h"
#include "tbx/file_system/json.h"

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
        EXPECT_NE(serialized.find("\"name\""), String::npos);
    }
}
