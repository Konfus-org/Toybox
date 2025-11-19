#include "pch.h"
#include "tbx/app/app_description.h"
#include <filesystem>
#include <string>
#include <vector>

namespace tbx::tests::app
{
    TEST(AppDescriptionTests, DefaultsAreEmpty)
    {
        AppDescription description = {};

        EXPECT_TRUE(description.name.empty());
        EXPECT_TRUE(description.working_root.empty());
        EXPECT_TRUE(description.assets_directory.empty());
        EXPECT_TRUE(description.logs_directory.empty());
        EXPECT_TRUE(description.plugins_directory.empty());
        EXPECT_TRUE(description.requested_plugins.empty());
    }

    TEST(AppDescriptionTests, StoresProvidedValues)
    {
        AppDescription description = AppDescription{
            "Toybox.Editor",
            std::filesystem::path("/workspace").string(),
            std::filesystem::path("assets").string(),
            std::filesystem::path("logs").string(),
            std::filesystem::path("plugins").string(),
            std::vector<std::string>{"Tbx.Logger", "Tbx.Renderer"}};

        EXPECT_EQ(description.name, "Toybox.Editor");
        EXPECT_EQ(description.working_root, std::filesystem::path("/workspace"));
        EXPECT_EQ(description.assets_directory, std::filesystem::path("assets"));
        EXPECT_EQ(description.logs_directory, std::filesystem::path("logs"));
        EXPECT_EQ(description.plugins_directory, std::filesystem::path("plugins"));
        ASSERT_EQ(description.requested_plugins.size(), 2u);
        EXPECT_EQ(description.requested_plugins[0], "Tbx.Logger");
        EXPECT_EQ(description.requested_plugins[1], "Tbx.Renderer");
    }
}
