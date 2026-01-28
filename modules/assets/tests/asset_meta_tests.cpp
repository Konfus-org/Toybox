#include "pch.h"
#include "test_filesystem.h"
#include "tbx/assets/asset_meta.h"

namespace tbx::tests::assets
{
    TEST(asset_meta_reader, reads_name_and_id_from_json)
    {
        TestFileSystem file_system;
        file_system.working_directory = "virtual";
        file_system.assets_directory = "virtual/assets";
        file_system.add_directory(file_system.assets_directory);
        file_system.add_file(
            "virtual/assets/hero.png.meta",
            R"({ "id": "1a2b", "name": "Hero" })");

        AssetMetaReader reader;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("virtual/assets/hero.png");

        ASSERT_TRUE(reader.try_read(file_system, asset_path, meta));
        EXPECT_EQ(meta.asset_path, asset_path);
        EXPECT_EQ(meta.name, "Hero");
        EXPECT_EQ(meta.id.value, 0x1a2bU);
    }

    TEST(asset_meta_reader, falls_back_to_stem_name)
    {
        TestFileSystem file_system;
        file_system.working_directory = "virtual";
        file_system.assets_directory = "virtual/assets";
        file_system.add_directory(file_system.assets_directory);
        file_system.add_file(
            "virtual/assets/rock.tbx.meta",
            R"({ "id": "10" })");

        AssetMetaReader reader;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("virtual/assets/rock.tbx");

        ASSERT_TRUE(reader.try_read(file_system, asset_path, meta));
        EXPECT_EQ(meta.name, "rock");
        EXPECT_EQ(meta.id.value, 0x10U);
    }
}
