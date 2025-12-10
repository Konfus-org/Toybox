#include "pch.h"
#include "tbx/app/app_description.h"
#include "tbx/file_system/filepath.h"
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
            FilePath("/workspace"),
            FilePath("assets"),
            FilePath("logs"),
            FilePath("plugins"),
            List<String>{"Tbx.Logger", "Tbx.Renderer"}};

        EXPECT_EQ(description.name.std_str(), "Toybox.Editor");
        EXPECT_EQ(description.working_root, FilePath("/workspace"));
        EXPECT_EQ(description.assets_directory, FilePath("assets"));
        EXPECT_EQ(description.logs_directory, FilePath("logs"));
        EXPECT_EQ(description.plugins_directory, FilePath("plugins"));
        ASSERT_EQ(description.requested_plugins.size(), 2u);
        EXPECT_EQ(description.requested_plugins[0].std_str(), "Tbx.Logger");
        EXPECT_EQ(description.requested_plugins[1].std_str(), "Tbx.Renderer");
    }
}
