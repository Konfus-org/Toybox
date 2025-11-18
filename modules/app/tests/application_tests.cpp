#include "pch.h"
#include "tbx/app/app_description.h"
#include "tbx/std/list.h"
#include "tbx/std/string.h"
#include <filesystem>

namespace tbx::tests::app
{
    TEST(AppDescriptionTests, DefaultsAreEmpty)
    {
        AppDescription description = {};

        EXPECT_TRUE(description.name.is_empty());
        EXPECT_TRUE(description.working_root.empty());
        EXPECT_TRUE(description.assets_directory.empty());
        EXPECT_TRUE(description.logs_directory.empty());
        EXPECT_TRUE(description.plugins_directory.empty());
        EXPECT_TRUE(description.requested_plugins.is_empty());
    }

    TEST(AppDescriptionTests, StoresProvidedValues)
    {
        AppDescription description = AppDescription{
            "Toybox.Editor",
            std::filesystem::path("/workspace"),
            std::filesystem::path("assets"),
            std::filesystem::path("logs"),
            std::filesystem::path("plugins"),
            tbx::List<tbx::String>{"Tbx.Logger", "Tbx.Renderer"}};

        EXPECT_EQ(description.name, "Toybox.Editor");
        EXPECT_EQ(description.working_root, std::filesystem::path("/workspace"));
        EXPECT_EQ(description.assets_directory, std::filesystem::path("assets"));
        EXPECT_EQ(description.logs_directory, std::filesystem::path("logs"));
        EXPECT_EQ(description.plugins_directory, std::filesystem::path("plugins"));
        ASSERT_EQ(description.requested_plugins.get_count(), 2u);
        EXPECT_EQ(description.requested_plugins[0], "Tbx.Logger");
        EXPECT_EQ(description.requested_plugins[1], "Tbx.Renderer");
    }
}
