#include "pch.h"
#include "tbx/assets/asset_handle_serializer.h"
#include "tbx/files/tests/in_memory_file_ops.h"
#include <filesystem>

namespace tbx::tests::assets
{
    using InMemoryFileOps = ::tbx::tests::file_system::InMemoryFileOps;

    TEST(asset_handle_serializer, reads_handle_from_ifileops_without_disk_io)
    {
        // Arrange
        AssetHandleSerializer parser;
        InMemoryFileOps file_ops("/virtual/assets");
        auto asset_path = std::filesystem::path("wood.png");
        std::string meta_text = "{ \"id\": \"99\" }\n";
        ASSERT_TRUE(file_ops.write_file("wood.png.meta", FileDataFormat::UTF8_TEXT, meta_text));

        // Act
        auto handle = parser.read_from_disk(file_ops, asset_path);

        // Assert
        ASSERT_NE(handle, nullptr);
        EXPECT_EQ(handle->id.value, 0x99U);
        EXPECT_EQ(handle->name, "wood.png");
    }
    TEST(asset_handle_serializer, preserves_invalid_id_when_meta_id_is_missing)
    {
        // Arrange
        AssetHandleSerializer parser;
        auto asset_path = std::filesystem::path("wood.png");
        std::string meta_text = "{ \"name\": \"Wood\" }\n";

        // Act
        auto handle = parser.read_from_source(meta_text, asset_path);

        // Assert
        ASSERT_NE(handle, nullptr);
        EXPECT_FALSE(handle->id.is_valid());
        EXPECT_EQ(handle->name, "wood.png");
    }
    TEST(asset_handle_serializer, writes_only_id_field_in_existing_meta)
    {
        // Arrange
        AssetHandleSerializer serializer;
        InMemoryFileOps file_ops("/virtual/assets");
        auto asset_path = std::filesystem::path("stone.png");
        std::string original_meta = R"JSON(
            {
              "name": "Stone",
              "texture": { "wrap": "repeat" },
              "id": "1"
            }
            )JSON";
        ASSERT_TRUE(
            file_ops.write_file("stone.png.meta", FileDataFormat::UTF8_TEXT, original_meta));
        auto updated_handle = Handle(Uuid(0xABU));

        // Act
        auto write_succeeded = serializer.try_write_to_disk(file_ops, asset_path, updated_handle);

        // Assert
        ASSERT_TRUE(write_succeeded);
        auto encoded = std::string();
        ASSERT_TRUE(file_ops.read_file("stone.png.meta", FileDataFormat::UTF8_TEXT, encoded));
        EXPECT_NE(encoded.find("\"id\": \"ab\""), std::string::npos);
        EXPECT_NE(encoded.find("\"name\": \"Stone\""), std::string::npos);
        EXPECT_NE(encoded.find("\"texture\""), std::string::npos);
    }
}
