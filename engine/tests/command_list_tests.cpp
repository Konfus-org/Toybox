#include "tbx/utils/command_list.h"
#include <array>
#include <gtest/gtest.h>

namespace tbx::tests
{
    TEST(CommandList, OptionsFlagsAndPositionalsParse)
    {
        // Arrange: both option forms, a bare flag, a list, and positionals around them.
        auto arguments = std::array {
            const_cast<char*>("app.exe"),
            const_cast<char*>("play.box"),
            const_cast<char*>("--width=1280"),
            const_cast<char*>("--title"),
            const_cast<char*>("My Game"),
            const_cast<char*>("--selftest"),
            const_cast<char*>("--layers=a, b,c"),
            const_cast<char*>("--scale"),
            const_cast<char*>("1.5"),
            const_cast<char*>("-w"),
            const_cast<char*>("640"),
            const_cast<char*>("-offset"),
            const_cast<char*>("-5")};

        // Act
        const auto commands = CommandList(static_cast<int>(arguments.size()), arguments.data());

        // Assert
        EXPECT_TRUE(commands.has("width"));
        EXPECT_EQ(commands.get<int>("width"), 1280);
        EXPECT_EQ(commands.get<std::string>("title"), "My Game");
        EXPECT_TRUE(commands.get<bool>("selftest"));
        EXPECT_NEAR(commands.get<float>("scale"), 1.5f, 0.0001f);
        EXPECT_EQ(commands.get<int>("w"), 640); // single-dash short option
        EXPECT_EQ(commands.get<int>("offset"), -5); // a negative number is a value, not an option
        const auto layers = commands.get_list<std::string>("layers");
        ASSERT_EQ(layers.size(), 3u);
        EXPECT_EQ(layers[0], "a");
        EXPECT_EQ(layers[1], "b");
        EXPECT_EQ(layers[2], "c");
        ASSERT_EQ(commands.get_positionals().size(), 1u);
        EXPECT_EQ(commands.get_positionals()[0], "play.box");
        EXPECT_EQ(
            commands.to_string(),
            "play.box --layers=a, b,c --offset=-5 --scale=1.5 --selftest=true --title=My Game "
            "--w=640 --width=1280");
    }

    TEST(CommandList, MissingAndMalformedOptionsFallBackToDefaults)
    {
        // Arrange: one malformed number and nothing else.
        auto arguments = std::array {
            const_cast<char*>("app.exe"),
            const_cast<char*>("--width=fast")};

        // Act
        const auto commands = CommandList(static_cast<int>(arguments.size()), arguments.data());

        // Assert: absent -> defaults, malformed -> default, absent list -> empty.
        EXPECT_FALSE(commands.has("height"));
        EXPECT_EQ(commands.get<int>("height", 900), 900);
        EXPECT_EQ(commands.get<int>("width", 640), 640);
        EXPECT_EQ(commands.get<std::string>("title", "fallback"), "fallback");
        EXPECT_TRUE(commands.get_list<int>("layers").empty());
        EXPECT_TRUE(commands.get_positionals().empty());
    }
}
