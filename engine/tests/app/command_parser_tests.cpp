#include "tbx/systems/app/command_list.h"
#include <gtest/gtest.h>

namespace tbx::tests::app
{
    TEST(command_list, parses_list_options_and_positionals)
    {
        // Arrange
        const auto argc = 7;
        char program[] = "ToyboxLauncher.exe";
        char app[] = "--app=ExampleApp";
        char working[] = "--working-dir=C:/Toybox/Apps/ThreeD";
        char settings[] = "--settings=Configs/ThreeDSettings.json";
        char option[] = "--load-plugins=ExampleApp, PerformanceMonitor";
        char flag[] = "--headless=true";
        char positional[] = "Example.world";
        char* argv[] = {program, app, working, settings, option, flag, positional};

        // Act
        const auto commands = CommandList(argc, argv);
        const auto plugins = commands.get_list<std::string>("load-plugins");

        // Assert
        EXPECT_EQ(commands.get<std::string>("app"), "ExampleApp");
        EXPECT_EQ(commands.get<std::string>("working-dir"), "C:/Toybox/Apps/ThreeD");
        EXPECT_EQ(commands.get<std::string>("settings"), "Configs/ThreeDSettings.json");
        ASSERT_EQ(plugins.size(), 2U);
        EXPECT_EQ(plugins[0], "ExampleApp");
        EXPECT_EQ(plugins[1], "PerformanceMonitor");
        EXPECT_TRUE(commands.has("headless"));
        ASSERT_EQ(commands.get_positionals().size(), 1U);
        EXPECT_EQ(commands.get_positionals()[0], "Example.world");
    }

    TEST(command_list, returns_defaults_for_missing_options)
    {
        // Arrange
        const auto argc = 1;
        char program[] = "ToyboxLauncher.exe";
        char* argv[] = {program};

        // Act
        const auto commands = CommandList(argc, argv);
        const auto plugins = commands.get_list<std::string>("load-plugins");
        const auto workers = commands.get<int>("workers", 4);

        // Assert
        EXPECT_TRUE(commands.get<std::string>("app").empty());
        EXPECT_TRUE(commands.get<std::string>("working-dir").empty());
        EXPECT_TRUE(commands.get<std::string>("settings").empty());
        EXPECT_FALSE(commands.has("load-plugins"));
        EXPECT_TRUE(plugins.empty());
        EXPECT_EQ(workers, 4);
        EXPECT_TRUE(commands.get_positionals().empty());
    }
}
