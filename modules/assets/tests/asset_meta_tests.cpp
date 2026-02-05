#include "pch.h"
#include "tbx/assets/asset_meta.h"
#include <filesystem>

namespace tbx::tests::assets
{
    TEST(asset_meta_parser, reads_name_and_id_from_json)
    {
        AssetMetaParser parser;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("/virtual/assets/hero.png");
        constexpr const char* meta_text = R"JSON({ "id": "1a2b", "name": "Hero" })JSON";

        ASSERT_TRUE(parser.try_parse_from_source(meta_text, asset_path, meta));
        EXPECT_EQ(meta.asset_path, asset_path);
        EXPECT_EQ(meta.name, "Hero");
        EXPECT_EQ(meta.id.value, 0x1a2bU);
    }

    TEST(asset_meta_parser, falls_back_to_stem_name)
    {
        AssetMetaParser parser;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("/virtual/assets/rock.tbx");
        constexpr const char* meta_text = R"JSON({ "id": "10" })JSON";

        ASSERT_TRUE(parser.try_parse_from_source(meta_text, asset_path, meta));
        EXPECT_EQ(meta.name, "rock");
        EXPECT_EQ(meta.id.value, 0x10U);
    }

    TEST(asset_meta_parser, parses_meta_text)
    {
        constexpr const char* meta_text = R"JSON({ "id": "a1", "name": "Crystal" })JSON";
        AssetMetaParser parser;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("/virtual/assets/crystal.asset");

        ASSERT_TRUE(parser.try_parse_from_source(meta_text, asset_path, meta));
        EXPECT_EQ(meta.asset_path, asset_path);
        EXPECT_EQ(meta.name, "Crystal");
        EXPECT_EQ(meta.id.value, 0xA1U);
    }

    TEST(asset_meta_parser, rejects_invalid_json)
    {
        AssetMetaParser parser;
        AssetMeta meta = {};
        const auto asset_path = std::filesystem::path("/virtual/assets/broken.asset");

        EXPECT_FALSE(parser.try_parse_from_source("{ invalid json }", asset_path, meta));
    }
}
